//! xuoproxy - development server for the CrossUO web build.
//!
//! One process, three jobs:
//!
//! 1. Static hosting of the wasm build (default `dist/wasm`) with the
//!    `Cross-Origin-Opener-Policy` / `Cross-Origin-Embedder-Policy` headers
//!    the pthread runtime requires (SharedArrayBuffer).
//!
//! 2. Same-origin reverse proxy for the streaming hosts, disk-cached:
//!    - `/patch/manifest/<path>` -> upstream manifest repo
//!    - `/patch/files/<path>`    -> upstream file repo
//!    - `/patch/eamythic/<path>` -> upstream aops root
//!    - `/stream/<url>`          -> generic: any absolute http(s) url
//!
//!    Works with any manifest type: proxied text bodies (MythicMFT, aops
//!    patchlists, JSON manifests) get their absolute repo urls rewritten to
//!    the same local prefixes, so manifest-referenced file repos and nested
//!    manifests stay on the proxy automatically. /stream additionally
//!    rewrites aops patchcfg ini bodies (the three repo keys) to
//!    `/stream/<encoded upstream url>`; binary bodies (zlib chunks, uop
//!    payloads, data files) pass through byte-identical.
//!
//! 3. A WebSocket-to-TCP bridge at `/ws/<host>/<port>`: the web client
//!    cannot open raw TCP sockets to shards, so it connects here and the
//!    bridge pipes binary websocket frames to a TCP stream (and back). The
//!    shard address is supplied by the client (the login server list), so
//!    any shard works.
//!
//! Usage:
//! ```text
//! xuoproxy [--listen 127.0.0.1:8777] [--dist dist/wasm]
//!          [--cache-dir xuoproxy-cache] [--cache-ttl <secs>] [--no-cache]
//!          [--manifest-upstream <url>] [--files-upstream <url>]
//!          [--aops-upstream <url>]
//! ```
//!
//! The web client boots against this server out of the box: the build default
//! `XUO_WEB_PRODUCT_URL` points at `/patch/manifest/...` (proxied), and shard
//! logins go through `/ws/<host>/<port>`.

mod normalize;

use std::collections::HashMap;
use std::path::PathBuf;
use std::sync::{Mutex, OnceLock};

use axum::extract::{ws::WebSocket, DefaultBodyLimit, Path, State, WebSocketUpgrade};
use axum::http::{header, HeaderMap, StatusCode};
use axum::response::{IntoResponse, Response};
use axum::routing::get;
use axum::Router;
use futures_util::{SinkExt, StreamExt};
use sha2::{Digest, Sha256};
use tokio::io::AsyncWriteExt;

static PKG_RE: OnceLock<regex::Regex> = OnceLock::new();
static MANIFEST_N_RE: OnceLock<regex::Regex> = OnceLock::new();

fn pkg_re() -> &'static regex::Regex {
    PKG_RE.get_or_init(|| regex::Regex::new("<package[^>]*>").unwrap())
}

fn manifest_n_re() -> &'static regex::Regex {
    MANIFEST_N_RE.get_or_init(|| regex::Regex::new("<manifest\\s[^>]*>").unwrap())
}

fn attr_of(tag: &str, name: &str) -> Option<String> {
    let needle = format!("{name}=\"");
    let at = tag.find(&needle)? + needle.len();
    let rest = &tag[at..];
    let end = rest.find('"')?;
    Some(rest[..end].to_string())
}

// normalized (all blocks stored, transferred uncompressed) whole-uop blob
// sizes per blob tail; filled by the manifest handler on first touch and by
// the startup warm pass - the manifest rewrite needs them so the client's
// plan, fetch caps and materialized sizes agree with what this proxy serves
fn normalized_sizes() -> &'static Mutex<HashMap<String, u64>> {
    static SIZES: OnceLock<Mutex<HashMap<String, u64>>> = OnceLock::new();
    SIZES.get_or_init(|| Mutex::new(HashMap::new()))
}

// ---------------------------------------------------------------------------
// configuration
// ---------------------------------------------------------------------------

#[derive(Clone, Debug)]
struct Config {
    listen: String,
    dist: PathBuf,
    cache_dir: PathBuf,
    cache_ttl: u64, // seconds; 0 = entries never expire
    use_cache: bool,
    upstream_manifest: String,
    upstream_files: String,
    upstream_aops: String,
}

// ---------------------------------------------------------------------------
// entry
// ---------------------------------------------------------------------------

#[tokio::main]
async fn main() {
    let mut listen = String::from("127.0.0.1:8777");
    let mut dist = String::from("dist/wasm");
    let mut cache_dir = String::from("xuoproxy-cache");
    let mut cache_ttl = 0u64;
    let mut use_cache = true;
    let mut upstream_manifest =
        String::from("http://patch.uo.broadsword.com/uopatch-sa/legacyrelease/uo/manifest");
    let mut upstream_files =
        String::from("http://patch.ak-cdn.eamythic.com/uopatch-sa/250/legacyrelease/uo/files");
    let mut upstream_aops =
        String::from("http://patch.uo.eamythic.com/uopatch-sa/legacyrelease/uo");

    let mut args = std::env::args().skip(1);
    while let Some(arg) = args.next() {
        let mut value = |name: &str| -> Option<String> {
            if arg == name {
                args.next()
            } else {
                arg.strip_prefix(&format!("{name}=")).map(str::to_string)
            }
        };
        if let Some(v) = value("--listen") {
            listen = v;
        } else if let Some(v) = value("--dist") {
            dist = v;
        } else if let Some(v) = value("--cache-dir") {
            cache_dir = v;
        } else if let Some(v) = value("--cache-ttl") {
            cache_ttl = v.parse().unwrap_or(0);
        } else if arg == "--no-cache" {
            use_cache = false;
        } else if let Some(v) = value("--manifest-upstream") {
            upstream_manifest = v;
        } else if let Some(v) = value("--files-upstream") {
            upstream_files = v;
        } else if let Some(v) = value("--aops-upstream") {
            upstream_aops = v;
        } else {
            eprintln!("unknown argument: {arg}");
            eprintln!("see --help");
            std::process::exit(2);
        }
    }

    let state = AppState {
        dist: PathBuf::from(&dist),
        cache_dir: PathBuf::from(&cache_dir),
        cache_ttl,
        use_cache,
        upstream_manifest: upstream_manifest.trim_end_matches('/').to_string(),
        upstream_files: upstream_files.trim_end_matches('/').to_string(),
        upstream_aops: upstream_aops.trim_end_matches('/').to_string(),
        http: reqwest::Client::builder()
            .connect_timeout(std::time::Duration::from_secs(30))
            .build()
            .expect("http client"),
    };

    let app = Router::new()
        .route(
            "/patch/manifest/*path",
            get(|State(st): State<AppState>, Path(path): Path<String>, headers: HeaderMap| async move {
                proxied(&st, &st.upstream_manifest, &path, &headers).await
            }),
        )
        .route(
            "/patch/files/*path",
            get(|State(st): State<AppState>, Path(path): Path<String>, headers: HeaderMap| async move {
                proxied(&st, &st.upstream_files, &path, &headers).await
            }),
        )
        .route(
            "/patch/eamythic/*path",
            get(|State(st): State<AppState>, Path(path): Path<String>, headers: HeaderMap| async move {
                proxied(&st, &st.upstream_aops, &path, &headers).await
            }),
        )
        .route("/stream/*url", get(stream_proxy))
        .route("/ws/:host/:port", get(ws_shard))
        .fallback(static_file)
        .layer(axum::middleware::from_fn(coop_coep))
        .layer(DefaultBodyLimit::max(512 * 1024 * 1024))
        .with_state(state.clone());

    // warm the .uop normalization cache in the background: fetch every
    // whole-file uop transfer the manifests declare, decode its blocks and
    // cache the plain container, so the first client boot already sees
    // normalized sizes in the manifests and range-able blobs on disk
    {
        let warm_state = state.clone();
        tokio::spawn(async move {
            // small startup grace so the listener is accepting first
            tokio::time::sleep(std::time::Duration::from_millis(250)).await;
            warm_uop_transfers(&warm_state).await;
        });
    }

    println!("xuoproxy listening on http://{listen}");
    println!("  static: {dist}");
    println!(
        "  proxy:  /patch/manifest|files|eamythic + /stream/<url> (cache: {})",
        cache_dir
    );
    println!("  bridge: /ws/<host>/<port>");
    let listener = tokio::net::TcpListener::bind(&listen).await.expect("bind");
    axum::serve(listener, app).await.expect("serve");
}

// ---------------------------------------------------------------------------
// uop transfer warm pass + manifest normalization
// ---------------------------------------------------------------------------

// fetches one whole-file uop transfer in normalized (all blocks stored,
// transferred uncompressed) form and caches it under its blob url
async fn ensure_uop_normalized(state: &AppState, blob_tail: &str) {
    let blob_url = format!("{}/{blob_tail}", state.upstream_files);
    if state.use_cache && cache_get(state, &blob_url).is_some() {
        return;
    }
    let started = std::time::Instant::now();
    let raw = match state.http.get(&blob_url).send().await {
        Ok(resp) => match resp.bytes().await {
            Ok(b) => b.to_vec(),
            Err(e) => {
                eprintln!("xuoproxy: warm {blob_tail} read failed: {e}");
                return;
            }
        },
        Err(e) => {
            eprintln!("xuoproxy: warm {blob_tail} fetch failed: {e}");
            return;
        }
    };
    let size_before = raw.len();
    let mut body = raw;
    if normalize::looks_zlib(&body) {
        if let Some(inflated) = normalize::zlib_inflate(&body) {
            body = inflated;
        }
    }
    if normalize::uop_needs_normalization(&body) {
        if let Some(normalized) = normalize::uop_normalize(&body) {
            body = normalized;
        }
    }
    let _ = std::fs::create_dir_all(&state.cache_dir);
    cache_put(state, &blob_url, 200, "application/octet-stream", &body);
    registered_sizes()
        .lock()
        .expect("sizes")
        .insert(blob_tail.to_string(), body.len() as u64);
    println!(
        "xuoproxy: warmed {blob_tail} ({} -> {} bytes) in {:.1}s",
        size_before,
        body.len(),
        started.elapsed().as_secs_f32()
    );
}

// walks the product manifest chain (prod -> pkg.mft -> *.mft) and warms
// every whole-file .uop transfer declared as a loose file
async fn warm_uop_transfers(state: &AppState) {
    let manifest_base = format!("{}/patch/manifest", "");
    let _ = manifest_base;
    let prod_url = format!(
        "{}/{}/{}",
        state.upstream_manifest, "legacyrelease", "uo-legacyrelease.prod"
    );
    // the upstream manifest base layout varies; try the configured repo root
    // with the same tail the client uses (the product url path)
    let prod_url = match state.upstream_manifest.rsplit_once('/') {
        Some((_, last)) if last == "manifest" => {
            format!("{}/uo-legacyrelease.prod", state.upstream_manifest)
        }
        _ => format!("{}/uo-legacyrelease.prod", state.upstream_manifest),
    };
    let _ = &prod_url;
    let body = match state.http.get(&prod_url).send().await {
        Ok(r) => match r.bytes().await {
            Ok(b) => b.to_vec(),
            Err(_) => return,
        },
        Err(_) => return,
    };
    let text = match std::str::from_utf8(&body) {
        Ok(t) => t.to_string(),
        Err(_) => match normalize::zlib_inflate(&body) {
            Some(t) => String::from_utf8_lossy(&t).into_owned(),
            None => return,
        },
    };

    // package rpaths + their pkg.mft names from the prod
    let mut queue: Vec<(String, String)> = Vec::new(); // (rpath, manifest name)
    for caps in pkg_re().find_iter(&text) {
        let tag = caps.as_str();
        let rpath = attr_of(tag, "rpath").unwrap_or_else(|| "base".into());
        let n = attr_of(tag, "n").unwrap_or_else(|| "pkg.mft".into());
        queue.push((rpath, n));
    }

    let mut uop_blobs: Vec<String> = Vec::new();
    let mut seen_manifests = std::collections::HashSet::new();
    while let Some((rpath, manifest_name)) = queue.pop() {
        let mft_url = format!("{}/{}/{}", state.upstream_manifest, rpath, manifest_name);
        if !seen_manifests.insert(mft_url.clone()) {
            continue;
        }
        let body = match state.http.get(&mft_url).send().await {
            Ok(r) => match r.bytes().await {
                Ok(b) => b.to_vec(),
                Err(_) => continue,
            },
            Err(_) => continue,
        };
        let text = match std::str::from_utf8(&body) {
            Ok(t) => t.to_string(),
            Err(_) => match normalize::zlib_inflate(&body) {
                Some(t) => String::from_utf8_lossy(&t).into_owned(),
                None => continue,
            },
        };
        // nested manifests - the regex matches the whole tag, the name is
        // an attribute (using the tag itself as the name fetched garbage
        // urls that failed silently: the warm pass warmed 0 transfers)
        for caps in manifest_n_re().find_iter(&text) {
            if let Some(n) = attr_of(caps.as_str(), "n") {
                queue.push((rpath.clone(), n));
            }
        }
        // loose .uop files
        for cap in normalize::manifest_loose_uop_tails(&text, &rpath) {
            uop_blobs.push(cap);
        }
    }

    println!("xuoproxy: warming {} whole-uop transfers", uop_blobs.len());
    for blob_tail in uop_blobs {
        ensure_uop_normalized(state, &blob_tail).await;
    }
}

// ---------------------------------------------------------------------------
// app state + middleware
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------

#[derive(Clone)]
struct AppState {
    dist: PathBuf,
    cache_dir: PathBuf,
    cache_ttl: u64,
    use_cache: bool,
    upstream_manifest: String,
    upstream_files: String,
    upstream_aops: String,
    http: reqwest::Client,
}

async fn coop_coep(req: axum::extract::Request, next: axum::middleware::Next) -> Response {
    let mut res = next.run(req).await;
    let headers = res.headers_mut();
    headers.insert(
        "Cross-Origin-Opener-Policy",
        "same-origin".parse().expect("static header"),
    );
    headers.insert(
        "Cross-Origin-Embedder-Policy",
        "require-corp".parse().expect("static header"),
    );
    res
}


// ---------------------------------------------------------------------------
// reverse proxy with manifest link rewriting
// ---------------------------------------------------------------------------

async fn proxied(
    state: &AppState,
    upstream_base: &str,
    tail: &str,
    headers: &HeaderMap,
) -> Response {
    let url = format!("{upstream_base}/{tail}");

    // files route: per-block punctual fetches depend on the proxy serving
    // normalized content (whole-transfer zlib inflated, uop containers
    // rebuilt with stored blocks) - see normalize.rs
    if upstream_base == state.upstream_manifest {
        return manifest_response(state, tail, headers).await;
    }
    if upstream_base == state.upstream_files {
        if state.use_cache {
            if let Some(range) = headers.get(header::RANGE).and_then(|v| v.to_str().ok()) {
                if let Some(resp) = cached_file_range(state, &url, range) {
                    return resp;
                }
            }
            if let Some((ctype, body)) = cached_file(state, &url) {
                // one content hash per serve: the 200 and its 206 slices
                // carry the same strong validator
                let etag = strong_etag(&body);
                if let Some(resp) = range_from_cached(&ctype, &body, headers, &etag) {
                    return resp;
                }
                return file_response(&ctype, body, headers, &etag);
            }
        }
        // full fetch on first touch: normalization needs the whole body, and
        // every later range is served from the normalized cache entry
        let body = match state.http.get(&url).send().await {
            Ok(resp) => match resp.bytes().await {
                Ok(b) => b.to_vec(),
                Err(e) => {
                    return (
                        StatusCode::BAD_GATEWAY,
                        format!("upstream read failed: {e}"),
                    )
                        .into_response()
                }
            },
            Err(e) => {
                return (
                    StatusCode::BAD_GATEWAY,
                    format!("upstream fetch failed: {e}"),
                )
                    .into_response()
            }
        };
        let body = normalize_file_body(tail, body);
        if state.use_cache {
            cache_put(state, &url, 200, "application/octet-stream", &body);
        }
        let etag = strong_etag(&body);
        if let Some(resp) = range_from_cached("application/octet-stream", &body, headers, &etag) {
            return resp;
        }
        return file_response("application/octet-stream", body, headers, &etag);
    }

    // disk cache first: repeated boots do not hit the upstream services
    if state.use_cache {
        // ranged request against a cached full body: read only the window
        // (the cache stores full 200 bodies; without this a cached entry
        // would shadow every range read, and a full read per range would
        // drag hundreds of MB through memcpy per block serve)
        if let Some(range) = headers.get(header::RANGE).and_then(|v| v.to_str().ok()) {
            if let Some(spec) = range.strip_prefix("bytes=") {
                if let Some((start_s, end_s)) = spec.split_once('-') {
                    if let (Ok(start), Ok(end)) = (start_s.parse::<usize>(), end_s.parse::<usize>())
                    {
                        if let Some((ctype, _etag, total, body)) =
                            cache_get_range_window(state, &url, start, end)
                        {
                            let ctype = sanitize_ctype(&ctype);
                            let rend = start + body.len().saturating_sub(1);
                            let mut res = (
                                StatusCode::PARTIAL_CONTENT,
                                [(header::CONTENT_TYPE, ctype)],
                                body,
                            )
                                .into_response();
                            res.headers_mut().insert(
                                header::CONTENT_RANGE,
                                format!("bytes {start}-{rend}/{total}")
                                    .parse()
                                    .expect("content-range"),
                            );
                            return res;
                        }
                    }
                }
            }
        }
        if let Some((status, ctype, _etag, body)) = cache_get(state, &url) {
            // ranged request against a cached full body: slice + 206 (the
            // cache only stores full 200 bodies; without this a cached entry
            // would shadow every range read - the web chunked fetch and the
            // block streaming both depend on real 206 answers)
            let etag = strong_etag(&body);
            if let Some(resp) = range_from_cached(&sanitize_ctype(&ctype), &body, headers, &etag) {
                return resp;
            }
            return respond_text_rewritten(status, &sanitize_ctype(&ctype), body, headers, None);
        }
    }

    let mut upstream = state.http.get(&url);
    if let Some(range) = headers.get(header::RANGE).and_then(|v| v.to_str().ok()) {
        upstream = upstream.header(header::RANGE, range);
    }
    let resp = match upstream.send().await {
        Ok(resp) => resp,
        Err(e) => {
            eprintln!("xuoproxy: upstream {url} failed: {e:?}");
            return (
                StatusCode::BAD_GATEWAY,
                format!("upstream fetch failed: {e}"),
            )
                .into_response();
        }
    };

    let status = resp.status().as_u16();
    // pass upstream validators through when present (binary payloads relay
    // byte-identical, so the upstream ETag stays a valid validator here);
    // rewritten bodies fall back to their own content hash below
    let upstream_etag = resp
        .headers()
        .get(header::ETAG)
        .and_then(|v| v.to_str().ok())
        .map(str::to_string);
    // the cdn occasionally serves content types with junk bytes on range
    // answers, which would fail HeaderValue parsing on every relay
    let ctype = sanitize_ctype(
        resp.headers()
            .get(header::CONTENT_TYPE)
            .and_then(|v| v.to_str().ok())
            .unwrap_or("application/octet-stream"),
    );
    let body = match resp.bytes().await {
        Ok(b) => b.to_vec(),
        Err(e) => {
            return (
                StatusCode::BAD_GATEWAY,
                format!("upstream read failed: {e}"),
            )
                .into_response()
        }
    };

    if state.use_cache && status == 200 {
        cache_put(state, &url, status, &sanitize_ctype(&ctype), &body);
    }

    respond_text_rewritten(
        status,
        &sanitize_ctype(&ctype),
        body,
        headers,
        upstream_etag.as_deref(),
    )
}

fn cache_entry(state: &AppState, url: &str) -> PathBuf {
    state.cache_dir.join(format!("{}.cache", sha256_hex(url.as_bytes())))
}

// cache entry v2 layout:
//   [status:2 BE][etag_len:4 BE][ctype_len:4 BE][etag][ctype][body]
// 10-byte fixed prefix (status + the two length fields); the etag is stored
// WITH its quotes (exactly as strong_etag produces it), so the whole-body 200
// and the windowed 206 answer the same URL with an identical validator without
// the 206 path reading (or hashing) the body.
fn cache_get(state: &AppState, url: &str) -> Option<(u16, String, String, Vec<u8>)> {
    let path = cache_entry(state, url);
    let raw = std::fs::read(&path).ok()?;

    if state.cache_ttl > 0 {
        if let Ok(meta) = std::fs::metadata(&path) {
            if let Ok(modified) = meta.modified() {
                if let Ok(age) = modified.elapsed() {
                    if age.as_secs() > state.cache_ttl {
                        return None;
                    }
                }
            }
        }
    }

    // 10-byte fixed prefix: status(2) + etag_len(4) + ctype_len(4)
    if raw.len() < 10 {
        return None;
    }
    let status = u16::from_be_bytes([raw[0], raw[1]]);
    let etag_len = u32::from_be_bytes([raw[2], raw[3], raw[4], raw[5]]) as usize;
    let ctype_len = u32::from_be_bytes([raw[6], raw[7], raw[8], raw[9]]) as usize;
    // deterministically reject v1 entries: their [2..6] is the ctype length and
    // the bytes that follow are ASCII ctype text, so the quote-delimited guard
    // on the etag field can never hold. A v1 entry is thus a miss, and the next
    // cache_put rewrites it as v2 - one re-download per entry, no migration pass.
    if etag_len == 0
        || etag_len > 128
        || raw.len() < 10 + etag_len
        || raw[10] != b'"'
        || raw[10 + etag_len - 1] != b'"'
    {
        return None;
    }
    let etag = String::from_utf8_lossy(&raw[10..10 + etag_len]).into_owned();
    let bo = 10 + etag_len;
    if raw.len() < bo + ctype_len {
        return None;
    }
    let ctype = String::from_utf8_lossy(&raw[bo..bo + ctype_len]).into_owned();
    let body = raw[bo + ctype_len..].to_vec();
    Some((status, ctype, etag, body))
}

fn cache_put(state: &AppState, url: &str, status: u16, ctype: &str, body: &[u8]) {
    let path = cache_entry(state, url);
    let etag = strong_etag(body);
    println!(
        "cache_put {} ({} bytes, {}, {})",
        path.display(),
        body.len(),
        ctype,
        etag
    );
    let _ = std::fs::create_dir_all(&state.cache_dir);
    let mut out = Vec::with_capacity(10 + etag.len() + ctype.len() + body.len());
    out.extend_from_slice(&status.to_be_bytes());
    out.extend_from_slice(&(etag.len() as u32).to_be_bytes());
    out.extend_from_slice(&(ctype.len() as u32).to_be_bytes());
    out.extend_from_slice(etag.as_bytes());
    out.extend_from_slice(ctype.as_bytes());
    out.extend_from_slice(body);
    let _ = std::fs::write(&path, &out);
}

// web punctual fetch support: per-blob normalization for the files route
fn normalize_file_body(tail: &str, body: Vec<u8>) -> Vec<u8> {
    // normalize ONLY plain uop containers whose blocks use encodings the
    // client's refine walk refuses (Flags == 3 BWT - gumpart ships that
    // way): rebuilt with every block stored, which turns the pack into an
    // ordinary range-able source. Everything else passes through
    // byte-identical - in particular ct=1 transfers (the client inflates
    // those itself, exactly like native).
    if normalize::uop_needs_normalization(&body) {
        let size_before = body.len();
        if let Some(normalized) = normalize::uop_normalize(&body) {
            println!(
                "xuoproxy: normalized uop container {tail} ({} -> {} bytes, blocks stored)",
                size_before,
                normalized.len()
            );
            return normalized;
        }
        eprintln!("xuoproxy: uop normalization FAILED for {tail} (serving raw)");
    }
    body
}

fn sanitize_ctype(ctype: &str) -> String {
    ctype
        .chars()
        .filter(|c| c.is_ascii_graphic() || *c == ' ')
        .collect()
}

// freshness policies: content-addressed artifacts (the hash-named /patch/files
// blobs and every pinned patch payload) never change per url -> immutable;
// manifests/patchlists change with every publish -> must revalidate
const CACHE_IMMUTABLE: &str = "public, max-age=31536000, immutable";
const CACHE_REVALIDATE: &str = "no-cache";

fn sha256_hex(data: &[u8]) -> String {
    let digest = Sha256::digest(data);
    digest.iter().map(|b| format!("{b:02x}")).collect()
}

// strong content validator (RFC 9110): quoted sha256 of the exact bytes this
// server returns for the resource
fn strong_etag(body: &[u8]) -> String {
    format!("\"{}\"", sha256_hex(body))
}

fn etag_matches(headers: &HeaderMap, etag: &str) -> bool {
    headers
        .get(header::IF_NONE_MATCH)
        .and_then(|v| v.to_str().ok())
        .map(|v| {
            v.split(',').any(|t| {
                let t = t.trim();
                t == "*" || t.strip_prefix("W/").unwrap_or(t) == etag
            })
        })
        .unwrap_or(false)
}

// conditional GET -> 304 with the same validators: the browser keeps its
// copy without re-downloading the body
fn not_modified_response(etag: &str, cache_control: &str) -> Response {
    let mut res = StatusCode::NOT_MODIFIED.into_response();
    let hs = res.headers_mut();
    hs.insert(header::ETAG, etag.parse().expect("etag"));
    hs.insert(
        header::CACHE_CONTROL,
        cache_control.parse().expect("cache-control"),
    );
    res
}

// 200 + freshness/validators shared by every /patch answer; `cache_control`
// picks the policy (CACHE_IMMUTABLE for content-addressed bodies,
// CACHE_REVALIDATE for manifests)
fn cacheable_response(
    status: StatusCode,
    ctype: &str,
    body: Vec<u8>,
    req_headers: &HeaderMap,
    etag: &str,
    cache_control: &str,
) -> Response {
    if etag_matches(req_headers, etag) {
        return not_modified_response(etag, cache_control);
    }
    let mut res = (
        status,
        [(header::CONTENT_TYPE, ctype.to_string())],
        body,
    )
        .into_response();
    let hs = res.headers_mut();
    hs.insert(header::ETAG, etag.parse().expect("etag"));
    hs.insert(
        header::CACHE_CONTROL,
        cache_control.parse().expect("cache-control"),
    );
    hs.insert(
        header::ACCEPT_RANGES,
        header::HeaderValue::from_static("bytes"),
    );
    res
}

// "bytes=start-end" (or "bytes=start-") against a cached full body -> a 206
// slice. Anything the client never sends (suffix ranges, multiple ranges,
// non-byte units) falls through to the full-body answer (None).
fn range_from_cached(
    ctype: &str,
    body: &[u8],
    headers: &HeaderMap,
    etag: &str,
) -> Option<Response> {
    let range = headers.get(header::RANGE)?.to_str().ok()?;
    let spec = range.strip_prefix("bytes=")?;
    let (start_s, end_s) = spec.split_once('-')?;
    let start: usize = start_s.parse().ok()?;
    let end: usize = if end_s.is_empty() {
        body.len().checked_sub(1)?
    } else {
        end_s.parse().ok()?
    };
    if start > end || end >= body.len() {
        return None;
    }
    let slice = body[start..=end].to_vec();
    let mut res = (
        StatusCode::PARTIAL_CONTENT,
        [(header::CONTENT_TYPE, ctype.to_string())],
        slice,
    )
        .into_response();
    res.headers_mut().insert(
        header::CONTENT_RANGE,
        format!("bytes {start}-{end}/{}", body.len())
            .parse()
            .expect("content-range"),
    );
    // 206 + validators is what lets browsers and CDNs treat partial answers
    // as reusable cache entries instead of one-shot downloads
    let hs = res.headers_mut();
    hs.insert(header::ETAG, etag.parse().expect("etag"));
    hs.insert(
        header::CACHE_CONTROL,
        CACHE_IMMUTABLE.parse().expect("cache-control"),
    );
    hs.insert(
        header::ACCEPT_RANGES,
        header::HeaderValue::from_static("bytes"),
    );
    Some(res)
}

fn respond_text_rewritten(
    status: u16,
    ctype: &str,
    body: Vec<u8>,
    headers: &HeaderMap,
    upstream_etag: Option<&str>,
) -> Response {
    let status = StatusCode::from_u16(status).unwrap_or(StatusCode::INTERNAL_SERVER_ERROR);
    let rewritten = rewrite_links(&body, headers);
    // a body that survived rewriting byte-identical (binary payloads) can
    // carry the upstream validator as-is; rewritten text bodies validate by
    // their own final hash
    let unrewritten = rewritten == body;
    let body = rewritten;
    if status != StatusCode::OK {
        return (status, [(header::CONTENT_TYPE, ctype.to_string())], body).into_response();
    }
    let etag = match upstream_etag {
        Some(etag) if unrewritten => etag.to_string(),
        _ => strong_etag(&body),
    };
    // generic-route payloads are versioned-repo artifacts the disk cache
    // already pins forever (cache_ttl 0): the same immutable + content-hash
    // treatment as /patch/files lets browsers reuse them, while the ETag
    // still answers explicit conditional GETs with a cheap 304
    cacheable_response(status, ctype, body, headers, &etag, CACHE_IMMUTABLE)
}

// ---------------------------------------------------------------------------
// manifest route: serves MythicMFT xml and normalizes whole-file .uop
// transfers for the web client
// ---------------------------------------------------------------------------

async fn manifest_response(state: &AppState, tail: &str, req_headers: &HeaderMap) -> Response {
    let url = format!("{}/{tail}", state.upstream_manifest);
    if state.use_cache {
        if let Some((ctype, body)) = cached_file(state, &url) {
            // the cached copy is exactly the body a fresh fetch would
            // produce: same validator + revalidation policy as below, so a
            // conditional GET 304s identically whichever path serves it
            let etag = strong_etag(&body);
            return cacheable_response(
                StatusCode::OK,
                &ctype,
                body,
                req_headers,
                &etag,
                CACHE_REVALIDATE,
            );
        }
    }

    let resp = match state.http.get(&url).send().await {
        Ok(resp) => resp,
        Err(e) => {
            return (
                StatusCode::BAD_GATEWAY,
                format!("upstream fetch failed: {e}"),
            )
                .into_response()
        }
    };
    let mut status = resp.status().as_u16();
    let ctype = resp
        .headers()
        .get(header::CONTENT_TYPE)
        .and_then(|v| v.to_str().ok())
        .unwrap_or("text/xml")
        .to_string();
    let mut body = match resp.bytes().await {
        Ok(b) => b.to_vec(),
        Err(e) => {
            return (
                StatusCode::BAD_GATEWAY,
                format!("upstream read failed: {e}"),
            )
                .into_response()
        }
    };

    // text manifests: rewrite upstream urls, then normalize every whole-file
    // .uop transfer this proxy serves plain (fetch once, decode all blocks,
    // cache) and declare the normalized size in the manifest - the client's
    // plan, fetch caps and materialized file sizes then match what the
    // proxy serves. Compressed NON-uop transfers (anim muls, statics, ...)
    // keep their ct: the client downloads and inflates those itself.
    //
    // Most manifests transfer ct=1 (zlib): inflate first, rewrite the text,
    // re-compress - the client picks the transform from the parent's ct and
    // expects a zlib stream. Only a REWRITTEN manifest changes bytes; a
    // compressed body that is not a manifest passes through untouched.
    let compressed = normalize::looks_zlib(&body);
    let mut plain = body.clone();
    if compressed {
        plain = normalize::zlib_inflate(&body).unwrap_or(body.clone());
    }
    if let Ok(text) = std::str::from_utf8(&plain) {
        if text.trim_start().starts_with('<') {
            let rewritten = rewrite_links(&plain, &HeaderMap::new());
            let text = String::from_utf8_lossy(&rewritten).into_owned();
            let rpath = tail.split('/').next().unwrap_or("base");
            let tails = normalize::manifest_loose_uop_tails(&text, rpath);
            for blob_tail in &tails {
                let blob_url = format!("{}/{blob_tail}", state.upstream_files);
                // already normalized and cached: record its size (the
                // manifest rewrite needs it) and skip the refetch
                if let Some(size) = cached_size(state, &blob_url) {
                    normalized_sizes()
                        .lock()
                        .expect("sizes")
                        .insert(blob_tail.clone(), size);
                    continue;
                }
                match fetch_normalized_blob(state, &blob_url, blob_tail).await {
                    Ok(size) => {
                        normalized_sizes()
                            .lock()
                            .expect("sizes")
                            .insert(blob_tail.clone(), size);
                    }
                    Err(e) => eprintln!("xuoproxy: normalize {blob_tail} failed: {e}"),
                }
            }
            let sizes = normalized_sizes().lock().expect("sizes").clone();
            let rewritten_text = normalize::rewrite_manifest_uop_entries(&text, &sizes);
            body = if compressed {
                normalize::zlib_deflate(&rewritten_text)
            } else {
                rewritten_text
            };
            status = 200;
            println!(
                "xuoproxy: manifest {tail} normalized ({} known sizes{}, {} bytes)",
                sizes.len(),
                if compressed {
                    ", ct=1 recompressed"
                } else {
                    ""
                },
                body.len()
            );
        }
    }

    if state.use_cache && status == 200 {
        cache_put(state, &url, status, "text/xml", &body);
    }
    let served_bytes = body.len();
    // manifests change with every publish: never let the browser trust its
    // copy blindly (no-cache), but hand it the content hash so an unchanged
    // revalidation is a cheap 304 instead of a full refetch
    let etag = strong_etag(&body);
    let res = if status == 200 {
        cacheable_response(
            StatusCode::OK,
            "text/xml",
            body,
            req_headers,
            &etag,
            CACHE_REVALIDATE,
        )
    } else {
        let mut res = (
            StatusCode::from_u16(status).unwrap_or(StatusCode::OK),
            body,
        )
            .into_response();
        res.headers_mut()
            .insert(header::CONTENT_TYPE, "text/xml".parse().expect("ctype"));
        res
    };
    if status == 200 {
        println!("xuoproxy: manifest {tail} served ({served_bytes} bytes, rewritten)");
    }
    res
}

/// fetches one blob from the files repo and returns it in normalized form:
/// whole-transfer zlib inflated, uop containers rebuilt with every block
/// stored. The normalized body is cached under the blob url.
async fn fetch_normalized_blob(
    state: &AppState,
    blob_url: &str,
    blob_tail: &str,
) -> Result<u64, String> {
    let resp = state
        .http
        .get(blob_url)
        .send()
        .await
        .map_err(|e| format!("upstream fetch failed: {e}"))?;
    if !resp.status().is_success() {
        return Err(format!("upstream status {}", resp.status().as_u16()));
    }
    let raw = resp
        .bytes()
        .await
        .map_err(|e| format!("upstream read failed: {e}"))?
        .to_vec();
    let body = normalize_file_body(blob_tail, raw);
    if state.use_cache {
        cache_put(state, blob_url, 200, "application/octet-stream", &body);
    }
    Ok(body.len() as u64)
}

// ---------------------------------------------------------------------------
// files route: normalized-content cache + response helpers
// ---------------------------------------------------------------------------

fn cached_file(state: &AppState, url: &str) -> Option<(String, Vec<u8>)> {
    let (status, ctype, _etag, body) = cache_get(state, url)?;
    if status != 200 {
        return None;
    }
    Some((sanitize_ctype(&ctype), body))
}

fn cached_size(state: &AppState, url: &str) -> Option<u64> {
    let (_, body) = cached_file(state, url)?;
    Some(body.len() as u64)
}

fn cached_file_range(state: &AppState, url: &str, range: &str) -> Option<Response> {
    let spec = range.strip_prefix("bytes=")?;
    let (start_s, end_s) = spec.split_once('-')?;
    let start: usize = start_s.parse().ok()?;
    let end_in: usize = if end_s.is_empty() {
        usize::MAX
    } else {
        end_s.parse().ok()?
    };
    let (ctype, etag, total, body) = cache_get_range_window(state, url, start, end_in)?;
    let end = start + body.len().saturating_sub(1);
    let mut res = (
        StatusCode::PARTIAL_CONTENT,
        [(header::CONTENT_TYPE, ctype)],
        body,
    )
        .into_response();
    res.headers_mut().insert(
        header::CONTENT_RANGE,
        format!("bytes {start}-{end}/{total}")
            .parse()
            .expect("range"),
    );
    // 206 + validators: the ETag is the stored whole-body strong validator, so
    // it is byte-identical to the one a 200 for the same URL carries
    let hs = res.headers_mut();
    hs.insert(header::ETAG, etag.parse().expect("etag"));
    hs.insert(
        header::CACHE_CONTROL,
        CACHE_IMMUTABLE.parse().expect("cache-control"),
    );
    hs.insert(
        header::ACCEPT_RANGES,
        header::HeaderValue::from_static("bytes"),
    );
    Some(res)
}

// windowed cache read; end_in may exceed the body (open-ended ranges)
fn cache_get_range_window(
    state: &AppState,
    url: &str,
    start: usize,
    end_in: usize,
) -> Option<(String, String, u64, Vec<u8>)> {
    let path = cache_entry(state, url);
    let mut f = std::fs::File::open(&path).ok()?;
    // 10-byte fixed prefix: status(2) + etag_len(4) + ctype_len(4)
    let mut hdr = [0u8; 10];
    std::io::Read::read_exact(&mut f, &mut hdr).ok()?;
    let etag_len = u32::from_be_bytes([hdr[2], hdr[3], hdr[4], hdr[5]]) as usize;
    if etag_len == 0 || etag_len > 128 {
        return None;
    }
    let ctype_len = u32::from_be_bytes([hdr[6], hdr[7], hdr[8], hdr[9]]) as usize;
    let mut etag_buf = vec![0u8; etag_len];
    std::io::Read::read_exact(&mut f, &mut etag_buf).ok()?;
    let etag = String::from_utf8_lossy(&etag_buf).into_owned();
    let mut ctype_buf = vec![0u8; ctype_len];
    std::io::Read::read_exact(&mut f, &mut ctype_buf).ok()?;
    let ctype = String::from_utf8_lossy(&ctype_buf).into_owned();
    let file_len = f.metadata().ok()?.len() as usize;
    let offset = 10 + etag_len + ctype_len;
    if file_len < offset {
        return None;
    }
    let body_len = (file_len - offset) as u64;
    let start64 = start as u64;
    if start64 >= body_len {
        return None;
    }
    let end = (end_in as u64).min(body_len.saturating_sub(1));
    if end < start64 {
        return None;
    }
    std::io::Seek::seek(&mut f, std::io::SeekFrom::Start(offset as u64 + start64)).ok()?;
    let len = (end - start64 + 1) as usize;
    let mut body = vec![0u8; len];
    std::io::Read::read_exact(&mut f, &mut body).ok()?;
    Some((ctype, etag, body_len, body))
}

// /patch/files answers are content-addressed (the url names a fixed artifact
// and normalized containers are rebuilt deterministically): immutable + a
// strong body-hash ETag so browsers and CDNs reuse them without revalidating
fn file_response(ctype: &str, body: Vec<u8>, req_headers: &HeaderMap, etag: &str) -> Response {
    cacheable_response(
        StatusCode::OK,
        ctype,
        body,
        req_headers,
        etag,
        CACHE_IMMUTABLE,
    )
}

// ---------------------------------------------------------------------------
// manifest normalization walk: whole-file .uop transfers are fetched once
// at startup, decoded (zlib blocks inflated, BWT blocks decoded) and cached
// plain; the manifests then declare the normalized sizes so the client's
// plan, fetch caps and materialized sizes all match what this proxy serves
// ---------------------------------------------------------------------------

fn registered_sizes() -> &'static Mutex<HashMap<String, u64>> {
    static SIZES: OnceLock<Mutex<HashMap<String, u64>>> = OnceLock::new();
    SIZES.get_or_init(|| Mutex::new(HashMap::new()))
}

fn normalized_manifest_url(manifest_url: &str) -> String {
    format!("{manifest_url}#normalized")
}

// inflate a ct=1 manifest transfer when its stored copy is still compressed
fn decompress_cached_manifest(state: &AppState, url: &str) {
    if let Some((_, _, _, body)) = cache_get(state, url) {
        if normalize::looks_zlib(&body) {
            if let Some(text) = normalize::zlib_inflate(&body) {
                cache_put(state, url, 200, "text/xml", &text);
            }
        }
    }
    let _ = state;
}

// ---------------------------------------------------------------------------
// manifest link rewriting: absolute upstream urls -> local proxy prefixes
// ---------------------------------------------------------------------------

static URL_RE: OnceLock<regex::Regex> = OnceLock::new();

fn rewrite_links(body: &[u8], headers: &HeaderMap) -> Vec<u8> {
    // only rewrite text bodies (product manifests, pkg.mft xml, patchlists);
    // binary payloads (zlib chunks, uop data) pass through byte-identical
    let text = match std::str::from_utf8(body) {
        Ok(text) => text,
        Err(_) => return body.to_vec(),
    };
    if !text.trim_start().starts_with('<') {
        return body.to_vec();
    }

    let re = URL_RE.get_or_init(|| regex::Regex::new(r#"https?://[^\s"'<>\\]+"#).unwrap());
    let replaced = re.replace_all(text, |caps: &regex::Captures| {
        let upstream = caps[0].trim_end_matches('/');
        // map each known upstream host to its local proxy prefix
        if upstream
            .starts_with("http://patch.uo.broadsword.com/uopatch-sa/legacyrelease/uo/manifest")
        {
            format!(
                "/patch/manifest/{}",
                upstream.trim_start_matches(
                    "http://patch.uo.broadsword.com/uopatch-sa/legacyrelease/uo/manifest"
                )
            )
        // any published patch version: the files repo layout is stable, only
        // the version segment moves (250, 251, ...) - pin whatever the live
        // manifest declares to the same-origin /patch/files lane (whose
        // upstream is picked by --files-upstream at startup)
        } else if upstream.starts_with("http://patch.ak-cdn.eamythic.com/uopatch-sa/")
            && upstream.contains("/legacyrelease/uo/files")
        {
            let after_version = upstream
                .split("uopatch-sa/")
                .nth(1)
                .and_then(|rest| rest.splitn(2, '/').nth(1))
                .unwrap_or("");
            format!(
                "/patch/files/{}",
                after_version.trim_start_matches("legacyrelease/uo/files")
            )
        } else if upstream.starts_with("http://patch.uo.eamythic.com/uopatch-sa/legacyrelease/uo") {
            format!(
                "/patch/eamythic/{}",
                upstream
                    .trim_start_matches("http://patch.uo.eamythic.com/uopatch-sa/legacyrelease/uo")
            )
        } else {
            caps[0].to_string()
        }
    });
    replaced.into_owned().into_bytes()
}

// ---------------------------------------------------------------------------
// /stream/<url>: generic same-origin url proxy (any manifest type)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// aops patchcfg rewrite: the ini's repo urls are absolute upstream addresses
// the browser cannot fetch cross-origin, so every value of the three repo
// keys is re-pointed at this proxy's /stream passthrough (percent-encoded,
// trailing slash kept literal - the client treats the value as a repo root
// and joins tails onto it). Relative values resolve against the patchcfg's
// own upstream root, mirroring the client's resolve(). The sniff only ever
// matches an ASCII ini: the UTF-16 patchlist and the binary data files pass
// through byte-identical. The rewritten body is what gets cached, so cache
// hits serve it verbatim (never re-rewritten).
// ---------------------------------------------------------------------------

const AOPS_REPO_KEYS: [&str; 5] = [
    "online_patch_directory",
    "patch_directory",
    "online_patchlist_file",
    "patchlist_url",
    "patch_salt_url",
];

// percent-encode everything outside the unreserved set (':' and '/' too) so
// the encoded url survives the wildcard capture; the route's decode returns
// exactly the upstream url
fn stream_url_encode(url: &str) -> String {
    let mut out = String::with_capacity(url.len() + 16);
    for byte in url.bytes() {
        match byte {
            b'A'..=b'Z' | b'a'..=b'z' | b'0'..=b'9' | b'-' | b'_' | b'.' | b'~' => {
                out.push(byte as char)
            }
            _ => out.push_str(&format!("%{byte:02X}")),
        }
    }
    out
}

fn stream_rewrite_aops(body: &[u8], upstream: &str) -> Vec<u8> {
    let text = match std::str::from_utf8(body) {
        Ok(text) => text,
        Err(_) => return body.to_vec(), // binary: never touched
    };
    // the root the client would resolve against: the patchcfg's own url up
    // to (and including) the last '/'
    let root = match upstream.rfind('/') {
        Some(at) => &upstream[..=at],
        None => return body.to_vec(),
    };
    let mut rewritten = false;
    let mut out = String::with_capacity(text.len() + 128);
    for line in text.split_inclusive('\n') {
        out.push_str(&stream_rewrite_aops_line(line, root, &mut rewritten));
    }
    if rewritten {
        out.into_bytes()
    } else {
        body.to_vec()
    }
}

fn stream_rewrite_aops_line(line: &str, root: &str, rewritten: &mut bool) -> String {
    let at = match line.find('=') {
        Some(at) => at,
        None => return line.to_string(),
    };
    let key = line[..at].trim();
    let value = line[at + 1..].trim();
    if !AOPS_REPO_KEYS.contains(&key) || value.is_empty() {
        return line.to_string();
    }
    // mirror the client's resolve(): absolute urls are kept, anything else
    // joins the root (which always ends with '/')
    let resolved = if value.starts_with("http://") || value.starts_with("https://") {
        value.to_string()
    } else {
        format!("{root}{}", value.trim_start_matches('/'))
    };
    // the patch directories are repo roots: keep the trailing slash the
    // client's parse enforces
    let mut url = resolved;
    if key.contains("patch_directory") && !url.ends_with('/') {
        url.push('/');
    }
    let slash = url.ends_with('/');
    let enc = stream_url_encode(if slash { &url[..url.len() - 1] } else { &url });
    // preserve the line's own ending (the client tolerates mixed endings)
    let ending = if line.ends_with("\r\n") {
        "\r\n"
    } else if line.ends_with('\n') {
        "\n"
    } else {
        ""
    };
    *rewritten = true;
    format!(
        "{key}=/stream/{enc}{}{ending}",
        if slash { "/" } else { "" }
    )
}

// full-body or ranged answer: the web transport only honors a 206 on its
// ranged reads (a 200 would re-download the whole file per window), so the
// cached body serves slices the same way the /patch/files lane does
fn stream_respond(status: u16, ctype: &str, body: Vec<u8>, headers: &HeaderMap) -> Response {
    if status == 200 && headers.contains_key(header::RANGE) {
        let etag = strong_etag(&body);
        if let Some(resp) = range_from_cached(ctype, &body, headers, &etag) {
            return resp;
        }
    }
    respond_plain(status, ctype, body)
}

async fn stream_proxy(
    State(state): State<AppState>,
    Path(url): Path<String>,
    headers: HeaderMap,
) -> Response {
    // axum already percent-decoded the wildcard; validate the scheme
    let url = percent_decode(&url);
    if !(url.starts_with("http://") || url.starts_with("https://")) {
        return (StatusCode::BAD_REQUEST, "expected an absolute http(s) url").into_response();
    }

    if state.use_cache {
        if let Some((status, ctype, _etag, body)) = cache_get(&state, &url) {
            return stream_respond(status, &ctype, body, &headers);
        }
    }

    let resp = match state.http.get(&url).send().await {
        Ok(resp) => resp,
        Err(e) => {
            return (
                StatusCode::BAD_GATEWAY,
                format!("upstream fetch failed: {e}"),
            )
                .into_response()
        }
    };

    let status = resp.status().as_u16();
    let ctype = resp
        .headers()
        .get(header::CONTENT_TYPE)
        .and_then(|v| v.to_str().ok())
        .unwrap_or("application/octet-stream")
        .to_string();
    let body = match resp.bytes().await {
        Ok(b) => b.to_vec(),
        Err(e) => {
            return (
                StatusCode::BAD_GATEWAY,
                format!("upstream read failed: {e}"),
            )
                .into_response()
        }
    };

    // an aops patchcfg is rewritten to same-origin /stream urls before it is
    // cached or served (200 only: error bodies pass through untouched)
    let body = if status == 200 {
        stream_rewrite_aops(&body, &url)
    } else {
        body
    };

    if status == 200 && state.use_cache {
        cache_put(&state, &url, status, &ctype, &body);
    }

    stream_respond(status, &ctype, body, &headers)
}

fn respond_plain(status: u16, ctype: &str, body: Vec<u8>) -> Response {
    let status = StatusCode::from_u16(status).unwrap_or(StatusCode::INTERNAL_SERVER_ERROR);
    (status, [(header::CONTENT_TYPE, ctype.to_string())], body).into_response()
}

fn percent_decode(text: &str) -> String {
    let bytes = text.as_bytes();
    let mut out = Vec::with_capacity(bytes.len());
    let mut i = 0;
    while i < bytes.len() {
        if bytes[i] == b'%' && i + 2 < bytes.len() + 1 && i + 3 <= bytes.len() {
            if let Ok(byte) = u8::from_str_radix(&text[i + 1..i + 3], 16) {
                out.push(byte);
                i += 3;
                continue;
            }
        }
        out.push(bytes[i]);
        i += 1;
    }
    String::from_utf8_lossy(&out).into_owned()
}

// ---------------------------------------------------------------------------
// /ws/<host>/<port>: WebSocket-to-TCP bridge (shard connections)
// ---------------------------------------------------------------------------

async fn ws_shard(Path((host, port)): Path<(String, u16)>, upgrade: WebSocketUpgrade) -> Response {
    upgrade.on_upgrade(move |socket| async move { bridge(socket, host, port).await })
}

async fn bridge(socket: WebSocket, host: String, port: u16) {
    let tcp = match tokio::net::TcpStream::connect((host.as_str(), port)).await {
        Ok(tcp) => tcp,
        Err(e) => {
            eprintln!("xuoproxy: ws bridge to {host}:{port} failed: {e}");
            return;
        }
    };
    println!("xuoproxy: ws bridge open -> {host}:{port}");

    let (mut ws_tx, mut ws_rx) = socket.split();
    let (mut tcp_rx, mut tcp_tx) = tokio::io::split(tcp);

    // client -> shard: binary websocket frames are raw protocol bytes
    let to_shard = async move {
        while let Some(Ok(msg)) = ws_rx.next().await {
            match msg {
                axum::extract::ws::Message::Binary(data) => {
                    if tcp_tx.write_all(&data).await.is_err() {
                        break;
                    }
                }
                axum::extract::ws::Message::Close(_) => break,
                _ => {}
            }
        }
        let _ = tcp_tx.shutdown().await;
    };

    // shard -> client: tcp bytes become binary websocket frames
    let to_client = async move {
        let mut buf = vec![0u8; 16384];
        loop {
            match tokio::io::AsyncReadExt::read(&mut tcp_rx, &mut buf).await {
                Ok(0) | Err(_) => break,
                Ok(n) => {
                    use tokio::io::AsyncWriteExt;
                    if ws_tx
                        .send(axum::extract::ws::Message::Binary(buf[..n].to_vec()))
                        .await
                        .is_err()
                    {
                        break;
                    }
                }
            }
        }
        let _ = ws_tx.close().await;
    };

    // whichever direction ends first tears the bridge down
    tokio::select! {
        _ = to_shard => {},
        _ = to_client => {},
    }
    println!("xuoproxy: ws bridge closed -> {host}:{port}");
}

// ---------------------------------------------------------------------------
// static wasm hosting
// ---------------------------------------------------------------------------

async fn static_file(
    State(state): State<AppState>,
    req_headers: axum::http::HeaderMap,
    uri: axum::http::Uri,
) -> Response {
    let path = uri.path();
    let rel = if path == "/" { "/index.html" } else { path };
    if rel.contains("..") {
        return (StatusCode::FORBIDDEN, "forbidden").into_response();
    }

    let full = state.dist.join(rel.trim_start_matches('/'));
    match tokio::fs::read(&full).await {
        Ok(bytes) => {
            let mime = mime_of(&full);
            // the dist artifacts share filenames across builds (crossuo.js /
            // crossuo.wasm are NOT content-addressed): any long-lived cache
            // eventually pairs an old artifact with a new one and the wasm
            // link fails ("import object field ... is not a Function").
            // Revalidate every load; the ETag keeps unchanged loads at 304.
            // (The content-addressed data lives under /patch/files, whose
            // hash-named responses stay immutable.)
            let etag = format!("\"{:016x}-{:x}\"", fnv1a64(&bytes), bytes.len());
            if req_headers
                .get(header::IF_NONE_MATCH)
                .and_then(|v| v.to_str().ok())
                .map(|v| v.split(',').any(|t| t.trim() == etag))
                .unwrap_or(false)
            {
                let mut not_modified = StatusCode::NOT_MODIFIED.into_response();
                not_modified
                    .headers_mut()
                    .insert(header::ETAG, etag.parse().expect("etag"));
                not_modified.headers_mut().insert(
                    header::CACHE_CONTROL,
                    "no-cache".parse().expect("cache-control"),
                );
                return not_modified;
            }
            let mut res = ([(header::CONTENT_TYPE, mime)], bytes).into_response();
            res.headers_mut()
                .insert(header::ETAG, etag.parse().expect("etag"));
            res.headers_mut().insert(
                header::CACHE_CONTROL,
                "no-cache".parse().expect("cache-control"),
            );
            res
        }
        Err(_) => (StatusCode::NOT_FOUND, "not found").into_response(),
    }
}

// FNV-1a 64: cheap content fingerprint for the ETag (64-bit is plenty to
// detect "the file changed" between builds)
fn fnv1a64(data: &[u8]) -> u64 {
    let mut hash: u64 = 0xcbf29ce484222325;
    for &b in data {
        hash ^= u64::from(b);
        hash = hash.wrapping_mul(0x100000001b3);
    }
    hash
}

fn mime_of(path: &std::path::Path) -> &'static str {
    match path.extension().and_then(|e| e.to_str()).unwrap_or("") {
        "html" => "text/html",
        "js" => "text/javascript",
        "mjs" => "text/javascript",
        "css" => "text/css",
        "wasm" => "application/wasm",
        "json" => "application/json",
        "png" => "image/png",
        "ico" => "image/x-icon",
        "data" => "application/octet-stream",
        "xml" | "prod" | "mft" => "text/xml",
        _ => "application/octet-stream",
    }
}

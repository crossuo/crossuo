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
//!    manifests stay on the proxy automatically. Binary bodies (zlib chunks,
//!    uop payloads) pass through byte-identical.
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

use std::path::PathBuf;
use std::sync::OnceLock;

use axum::extract::{ws::WebSocket, DefaultBodyLimit, Path, State, WebSocketUpgrade};
use axum::http::{header, HeaderMap, StatusCode};
use axum::response::{IntoResponse, Response};
use axum::routing::get;
use axum::Router;
use futures_util::{SinkExt, StreamExt};
use tokio::io::AsyncWriteExt;
use sha2::{Digest, Sha256};

// ---------------------------------------------------------------------------
// configuration
// ---------------------------------------------------------------------------

#[derive(Clone, Debug)]
struct Config
{
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
async fn main()
{
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
    while let Some(arg) = args.next()
    {
        let mut value = |name: &str| -> Option<String> {
            if arg == name
            {
                args.next()
            }
            else
            {
                arg.strip_prefix(&format!("{name}=")).map(str::to_string)
            }
        };
        if let Some(v) = value("--listen")
        {
            listen = v;
        }
        else if let Some(v) = value("--dist")
        {
            dist = v;
        }
        else if let Some(v) = value("--cache-dir")
        {
            cache_dir = v;
        }
        else if let Some(v) = value("--cache-ttl")
        {
            cache_ttl = v.parse().unwrap_or(0);
        }
        else if arg == "--no-cache"
        {
            use_cache = false;
        }
        else if let Some(v) = value("--manifest-upstream")
        {
            upstream_manifest = v;
        }
        else if let Some(v) = value("--files-upstream")
        {
            upstream_files = v;
        }
        else if let Some(v) = value("--aops-upstream")
        {
            upstream_aops = v;
        }
        else
        {
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
        .with_state(state);

    println!("xuoproxy listening on http://{listen}");
    println!("  static: {dist}");
    println!("  proxy:  /patch/manifest|files|eamythic + /stream/<url> (cache: {})", cache_dir);
    println!("  bridge: /ws/<host>/<port>");
    let listener = tokio::net::TcpListener::bind(&listen).await.expect("bind");
    axum::serve(listener, app).await.expect("serve");
}

// ---------------------------------------------------------------------------
// app state + middleware
// ---------------------------------------------------------------------------

#[derive(Clone)]
struct AppState
{
    dist: PathBuf,
    cache_dir: PathBuf,
    cache_ttl: u64,
    use_cache: bool,
    upstream_manifest: String,
    upstream_files: String,
    upstream_aops: String,
    http: reqwest::Client,
}

async fn coop_coep(req: axum::extract::Request, next: axum::middleware::Next) -> Response
{
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

async fn proxied(state: &AppState, upstream_base: &str, tail: &str, headers: &HeaderMap) -> Response
{
    let url = format!("{upstream_base}/{tail}");

    // disk cache first: repeated boots do not hit the upstream services
    if state.use_cache
    {
        // ranged request against a cached full body: read only the window
        // (the cache stores full 200 bodies; without this a cached entry
        // would shadow every range read, and a full read per range would
        // drag hundreds of MB through memcpy per block serve)
        if let Some(range) = headers.get(header::RANGE).and_then(|v| v.to_str().ok())
        {
            if let Some(spec) = range.strip_prefix("bytes=")
            {
                if let Some((start_s, end_s)) = spec.split_once('-')
                {
                    if let (Ok(start), Ok(end)) = (start_s.parse::<usize>(), end_s.parse::<usize>())
                    {
                        if let Some((status, ctype, total, body)) =
                            cache_get_range(state, &url, start, end)
                        {
                            let ctype = sanitize_ctype(&ctype);
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
                                    .expect("content-range"),
                            );
                            return res;
                        }
                    }
                }
            }
        }
        if let Some((status, ctype, body)) = cache_get(state, &url)
        {
            // ranged request against a cached full body: slice + 206 (the
            // cache only stores full 200 bodies; without this a cached entry
            // would shadow every range read - the web chunked fetch and the
            // block streaming both depend on real 206 answers)
            if let Some(resp) = range_from_cached(&sanitize_ctype(&ctype), &body, headers)
            {
                return resp;
            }
            return respond_text_rewritten(status, &sanitize_ctype(&ctype), body, headers);
        }
    }

    let mut upstream = state.http.get(&url);
    if let Some(range) = headers.get(header::RANGE).and_then(|v| v.to_str().ok())
    {
        upstream = upstream.header(header::RANGE, range);
    }
    let resp = match upstream.send().await
    {
        Ok(resp) => resp,
        Err(e) =>
        {
            eprintln!("xuoproxy: upstream {url} failed: {e:?}");
            return (
                StatusCode::BAD_GATEWAY,
                format!("upstream fetch failed: {e}"),
            )
                .into_response()
        }
    };

    let status = resp.status().as_u16();
    // the cdn occasionally serves content types with junk bytes on range
    // answers, which would fail HeaderValue parsing on every relay
    let ctype = sanitize_ctype(
        resp.headers()
            .get(header::CONTENT_TYPE)
            .and_then(|v| v.to_str().ok())
            .unwrap_or("application/octet-stream"),
    );
    let body = match resp.bytes().await
    {
        Ok(b) => b.to_vec(),
        Err(e) => return (StatusCode::BAD_GATEWAY, format!("upstream read failed: {e}")).into_response(),
    };

    if state.use_cache && status == 200
    {
        cache_put(state, &url, status, &sanitize_ctype(&ctype), &body);
    }

    respond_text_rewritten(status, &sanitize_ctype(&ctype), body, headers)
}

fn sanitize_ctype(ctype: &str) -> String
{
    ctype
        .chars()
        .filter(|c| c.is_ascii_graphic() || *c == ' ')
        .collect()
}

// "bytes=start-end" (or "bytes=start-") against a cached full body -> a 206
// slice. Anything the client never sends (suffix ranges, multiple ranges,
// non-byte units) falls through to the full-body answer (None).
fn range_from_cached(ctype: &str, body: &[u8], headers: &HeaderMap) -> Option<Response>
{
    let range = headers.get(header::RANGE)?.to_str().ok()?;
    let spec = range.strip_prefix("bytes=")?;
    let (start_s, end_s) = spec.split_once('-')?;
    let start: usize = start_s.parse().ok()?;
    let end: usize = if end_s.is_empty()
    {
        body.len().checked_sub(1)?
    }
    else
    {
        end_s.parse().ok()?
    };
    if start > end || end >= body.len()
    {
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
    Some(res)
}

fn respond_text_rewritten(status: u16, ctype: &str, body: Vec<u8>, headers: &HeaderMap) -> Response
{
    let status = StatusCode::from_u16(status).unwrap_or(StatusCode::INTERNAL_SERVER_ERROR);
    let body = rewrite_links(&body, headers);
    (status, [(header::CONTENT_TYPE, ctype.to_string())], body).into_response()
}

// ---------------------------------------------------------------------------
// disk cache (keyed by url; raw upstream bodies, rewrites applied per serve)
// ---------------------------------------------------------------------------

fn cache_entry(state: &AppState, url: &str) -> PathBuf
{
    let digest = Sha256::digest(url.as_bytes());
    let hex: String = digest.iter().map(|b| format!("{b:02x}")).collect();
    state.cache_dir.join(format!("{hex}.cache"))
}

fn cache_get(state: &AppState, url: &str) -> Option<(u16, String, Vec<u8>)>
{
    let path = cache_entry(state, url);
    let raw = std::fs::read(&path).ok()?;

    if state.cache_ttl > 0
    {
        if let Ok(meta) = std::fs::metadata(&path)
        {
            if let Ok(modified) = meta.modified()
            {
                if let Ok(age) = modified.elapsed()
                {
                    if age.as_secs() > state.cache_ttl
                    {
                        return None;
                    }
                }
            }
        }
    }

    if raw.len() < 6
    {
        return None;
    }
    let status = u16::from_be_bytes([raw[0], raw[1]]);
    let ctype_len = u32::from_be_bytes([raw[2], raw[3], raw[4], raw[5]]) as usize;
    if raw.len() < 6 + ctype_len
    {
        return None;
    }
    let ctype = String::from_utf8_lossy(&raw[6..6 + ctype_len]).into_owned();
    let body = raw[6 + ctype_len..].to_vec();
    Some((status, ctype, body))
}

// windowed cache read: [start, end] of the cached body without loading the
// whole file (ranged serves hit multi-hundred-MB entries thousands of times
// per boot; each full read is pure memcpy overhead). Also returns the total
// body length for the Content-Range header.
fn cache_get_range(
    state: &AppState,
    url: &str,
    start: usize,
    end_in: usize,
) -> Option<(u16, String, u64, Vec<u8>)>
{
    let path = cache_entry(state, url);
    let mut f = std::fs::File::open(&path).ok()?;

    if state.cache_ttl > 0
    {
        if let Ok(meta) = f.metadata()
        {
            if let Ok(modified) = meta.modified()
            {
                if let Ok(age) = modified.elapsed()
                {
                    if age.as_secs() > state.cache_ttl
                    {
                        return None;
                    }
                }
            }
        }
    }

    let mut hdr = [0u8; 6];
    std::io::Read::read_exact(&mut f, &mut hdr).ok()?;
    let status = u16::from_be_bytes([hdr[0], hdr[1]]);
    let ctype_len = u32::from_be_bytes([hdr[2], hdr[3], hdr[4], hdr[5]]) as usize;
    let mut ctype_buf = vec![0u8; ctype_len];
    std::io::Read::read_exact(&mut f, &mut ctype_buf).ok()?;
    let ctype = String::from_utf8_lossy(&ctype_buf).into_owned();

    let file_len = f.metadata().ok()?.len() as usize;
    if file_len < 6 + ctype_len || start > end_in
    {
        return None;
    }
    let body_len = file_len - 6 - ctype_len;
    let end = end_in.min(body_len.saturating_sub(1));
    if start > end
    {
        return None;
    }
    std::io::Seek::seek(
        &mut f,
        std::io::SeekFrom::Start((6 + ctype_len + start) as u64),
    )
    .ok()?;
    let mut body = vec![0u8; end - start + 1];
    std::io::Read::read_exact(&mut f, &mut body).ok()?;
    Some((status, ctype, body_len as u64, body))
}

fn cache_put(state: &AppState, url: &str, status: u16, ctype: &str, body: &[u8])
{
    let path = cache_entry(state, url);
    println!("cache_put {} ({} bytes, {})", path.display(), body.len(), ctype);
    let _ = std::fs::create_dir_all(&state.cache_dir);
    let path = cache_entry(state, url);
    let mut out = Vec::with_capacity(8 + ctype.len() + body.len());
    out.extend_from_slice(&status.to_be_bytes());
    out.extend_from_slice(&(ctype.len() as u32).to_be_bytes());
    out.extend_from_slice(ctype.as_bytes());
    out.extend_from_slice(body);
    let _ = std::fs::write(&path, &out);
}

// ---------------------------------------------------------------------------
// manifest link rewriting: absolute upstream urls -> local proxy prefixes
// ---------------------------------------------------------------------------

static URL_RE: OnceLock<regex::Regex> = OnceLock::new();

fn rewrite_links(body: &[u8], headers: &HeaderMap) -> Vec<u8>
{
    // only rewrite text bodies (product manifests, pkg.mft xml, patchlists);
    // binary payloads (zlib chunks, uop data) pass through byte-identical
    let text = match std::str::from_utf8(body)
    {
        Ok(text) => text,
        Err(_) => return body.to_vec(),
    };
    if !text.trim_start().starts_with('<')
    {
        return body.to_vec();
    }

    let re = URL_RE.get_or_init(|| regex::Regex::new(r#"https?://[^\s"'<>\\]+"#).unwrap());
    let replaced = re.replace_all(text, |caps: &regex::Captures| {
        let upstream = caps[0].trim_end_matches('/');
        // map each known upstream host to its local proxy prefix
        if upstream.starts_with("http://patch.uo.broadsword.com/uopatch-sa/legacyrelease/uo/manifest")
        {
            format!("/patch/manifest/{}", upstream.trim_start_matches("http://patch.uo.broadsword.com/uopatch-sa/legacyrelease/uo/manifest"))
        }
        else if upstream.starts_with("http://patch.ak-cdn.eamythic.com/uopatch-sa/250/legacyrelease/uo/files")
        {
            format!("/patch/files/{}", upstream.trim_start_matches("http://patch.ak-cdn.eamythic.com/uopatch-sa/250/legacyrelease/uo/files"))
        }
        else if upstream.starts_with("http://patch.uo.eamythic.com/uopatch-sa/legacyrelease/uo")
        {
            format!("/patch/eamythic/{}", upstream.trim_start_matches("http://patch.uo.eamythic.com/uopatch-sa/legacyrelease/uo"))
        }
        else
        {
            caps[0].to_string()
        }
    });
    replaced.into_owned().into_bytes()
}

// ---------------------------------------------------------------------------
// /stream/<url>: generic same-origin url proxy (any manifest type)
// ---------------------------------------------------------------------------

async fn stream_proxy(State(state): State<AppState>, Path(url): Path<String>) -> Response
{
    // axum already percent-decoded the wildcard; validate the scheme
    let url = percent_decode(&url);
    if !(url.starts_with("http://") || url.starts_with("https://"))
    {
        return (StatusCode::BAD_REQUEST, "expected an absolute http(s) url").into_response();
    }

    if state.use_cache
    {
        if let Some((status, ctype, body)) = cache_get(&state, &url)
        {
            return respond_plain(status, &ctype, body);
        }
    }

    let resp = match state.http.get(&url).send().await
    {
        Ok(resp) => resp,
        Err(e) => return (StatusCode::BAD_GATEWAY, format!("upstream fetch failed: {e}")).into_response(),
    };

    let status = resp.status().as_u16();
    let ctype = resp
        .headers()
        .get(header::CONTENT_TYPE)
        .and_then(|v| v.to_str().ok())
        .unwrap_or("application/octet-stream")
        .to_string();
    let body = match resp.bytes().await
    {
        Ok(b) => b.to_vec(),
        Err(e) => return (StatusCode::BAD_GATEWAY, format!("upstream read failed: {e}")).into_response(),
    };

    if status == 200 && state.use_cache
    {
        cache_put(&state, &url, status, &ctype, &body);
    }

    respond_plain(status, &ctype, body)
}

fn respond_plain(status: u16, ctype: &str, body: Vec<u8>) -> Response
{
    let status = StatusCode::from_u16(status).unwrap_or(StatusCode::INTERNAL_SERVER_ERROR);
    (status, [(header::CONTENT_TYPE, ctype.to_string())], body).into_response()
}

fn percent_decode(text: &str) -> String
{
    let bytes = text.as_bytes();
    let mut out = Vec::with_capacity(bytes.len());
    let mut i = 0;
    while i < bytes.len()
    {
        if bytes[i] == b'%' && i + 2 < bytes.len() + 1 && i + 3 <= bytes.len()
        {
            if let Ok(byte) = u8::from_str_radix(&text[i + 1..i + 3], 16)
            {
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

async fn ws_shard(
    Path((host, port)): Path<(String, u16)>,
    upgrade: WebSocketUpgrade,
) -> Response
{
    upgrade.on_upgrade(move |socket| async move { bridge(socket, host, port).await })
}

async fn bridge(socket: WebSocket, host: String, port: u16)
{
    let tcp = match tokio::net::TcpStream::connect((host.as_str(), port)).await
    {
        Ok(tcp) => tcp,
        Err(e) =>
        {
            eprintln!("xuoproxy: ws bridge to {host}:{port} failed: {e}");
            return;
        }
    };
    println!("xuoproxy: ws bridge open -> {host}:{port}");

    let (mut ws_tx, mut ws_rx) = socket.split();
    let (mut tcp_rx, mut tcp_tx) = tokio::io::split(tcp);

    // client -> shard: binary websocket frames are raw protocol bytes
    let to_shard = async move
    {
        while let Some(Ok(msg)) = ws_rx.next().await
        {
            match msg
            {
                axum::extract::ws::Message::Binary(data) =>
                {
                    if tcp_tx.write_all(&data).await.is_err()
                    {
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
    let to_client = async move
    {
        let mut buf = vec![0u8; 16384];
        loop
        {
            match tokio::io::AsyncReadExt::read(&mut tcp_rx, &mut buf).await
            {
                Ok(0) | Err(_) => break,
                Ok(n) =>
                {
                    use tokio::io::AsyncWriteExt;
                    if ws_tx.send(axum::extract::ws::Message::Binary(buf[..n].to_vec())).await.is_err()
                    {
                        break;
                    }
                }
            }
        }
        let _ = ws_tx.close().await;
    };

    // whichever direction ends first tears the bridge down
    tokio::select!
    {
        _ = to_shard => {},
        _ = to_client => {},
    }
    println!("xuoproxy: ws bridge closed -> {host}:{port}");
}

// ---------------------------------------------------------------------------
// static wasm hosting
// ---------------------------------------------------------------------------

async fn static_file(State(state): State<AppState>, uri: axum::http::Uri) -> Response
{
    let path = uri.path();
    let rel = if path == "/" { "/index.html" } else { path };
    if rel.contains("..")
    {
        return (StatusCode::FORBIDDEN, "forbidden").into_response();
    }

    let full = state.dist.join(rel.trim_start_matches('/'));
    match tokio::fs::read(&full).await
    {
        Ok(bytes) =>
        {
            let mime = mime_of(&full);
            ([(header::CONTENT_TYPE, mime)], bytes).into_response()
        }
        Err(_) => (StatusCode::NOT_FOUND, "not found").into_response(),
    }
}

fn mime_of(path: &std::path::Path) -> &'static str
{
    match path.extension().and_then(|e| e.to_str()).unwrap_or("")
    {
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

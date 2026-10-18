//! xuoproxy - development server for the CrossUO web build.
//!
//! One process, three jobs:
//!
//! 1. Static hosting of the wasm build (default `html/`) with the
//!    `Cross-Origin-Opener-Policy` / `Cross-Origin-Embedder-Policy` headers
//!    the pthread runtime requires (SharedArrayBuffer).
//!
//! 2. Same-origin reverse proxy for the streaming hosts, disk-cached:
//!    - `/patch/manifest/<path>` -> upstream manifest repo
//!    - `/patch/files/<path>`    -> upstream file repo
//!    - `/patch/eamythic/<path>` -> upstream aops root
//!    - `/stream/<url>`          -> generic: any absolute http(s) url
//!
//!    Plus the whole-blob window route for cacheable ranged reads:
//!    `/blob/<urlhash>/<offset>/<size>?u=<url>` answers 200 with exactly the
//!    `[offset, offset+size)` window of the upstream resource (clamped at
//!    EOF) under the same immutable treatment as whole files - plain 200s
//!    every intermediate cache can store, instead of one-shot 206s. The
//!    client always sends `u`: the proxy hashes it (the disk-cache key) and
//!    rejects a mismatch with the path hash.
//!
//!    Content freshness: a disk-cached body is only as current as its last
//!    upstream check, so manifest-flavored entries (the MythicMFT product
//!    uo-legacyrelease.prod / sub-manifests on /patch/manifest,
//!    aops patchlists and patchcfgs on /stream and the generic route) are
//!    never served blind. Every request - throttled by `--manifest-ttl <secs>`
//!    (default 0 = check every request; manifests are KBs and upstreams are
//!    CDNs) - revalidates the entry with a conditional GET (If-None-Match /
//!    If-Modified-Since built from the upstream validators captured at cache
//!    time, persisted in a small metadata tail on the cache entry; tail-less
//!    entries = no validators, treated as fresh, so old caches keep working).
//!    Upstream 304 -> the cached (rewritten) body serves; upstream 200 -> the
//!    body re-normalizes, re-caches and - when it actually changed - the
//!    content entries the OLD body referenced are invalidated so changed data
//!    re-fetches fresh.
//!
//!    Browser cache busting: every rewritten resource url carries a version
//!    token, `...?v=<token>` (per-entry manifest attrs when available, the
//!    manifest-level validator otherwise). The token is BROWSER-FACING ONLY:
//!    every incoming url resolution strips `?v=<token>` (and any other `v`
//!    param) before computing a cache key or upstream url - see
//!    strip_version_param, the linchpin of the scheme.
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
//! 4. Local data mounts at `/data/<name>/<path>`: a shard owner keeps the
//!    ORIGINAL client install on the proxy host. Two ways to mount:
//!    `--data <name>=<dir>` (repeatable - one mount per shard, so hub
//!    deployments expose many), and shards.cfg entries whose producturl
//!    points at a server-side folder, `Installer=local:<path>` (absolute,
//!    or relative to the root tree). Those are auto-mounted under a name
//!    derived from the shard's own Name, and the served shards.cfg carries
//!    the same-origin product url in the producturl field, so any deployment
//!    exposing local folders needs no flags beyond --root. A remote CDN url
//!    keeps working unchanged.
//!
//!    A mount speaks MythicMFT: when the folder has no manifest on disk, the
//!    proxy SYNTHESIZES a MYP product for it - a generated product manifest
//!    (served at the `<root>manifest` url the client always requests, and
//!    dispatched on content like the official one), one generated
//!    `base/pkg.mft` sub-manifest listing the walk of the folder (the same
//!    UO-data file filter the client applies), and loose-file blobs at
//!    `base/unpacked/<name hash>` (lookup3 of the lowercased name, the exact
//!    resource scheme the official servers use). Entries are stored (ct=0);
//!    big .uop files refine into ranged block assets client-side. The
//!    generated product carries a content fingerprint as its serial, so a
//!    changed folder re-plans the client on the next boot. An EXISTING
//!    manifest on disk (a real aops mirror's patchlist/patchcfg, say) is
//!    always served byte-identical instead - no synthesis, no rewriting.
//!    Data files serve whole (streamed) and ranged (206), case-insensitively
//!    on the last path component (the client requests lowercased basenames;
//!    legacy installs ship `ART.MUL`-style names). Blob windows
//!    (`/blob/...?u=/data/...`) read straight off the mounted file - local
//!    disk needs no second cache.
//!
//! Usage:
//! ```text
//! xuoproxy [--listen 127.0.0.1:8777] [--root html/]
//!          [--cache-dir xuoproxy-cache] [--cache-ttl <secs>] [--no-cache]
//!          [--manifest-ttl <secs>] [--upstream-timeout <secs>]
//!          [--manifest-upstream <url>] [--files-upstream <url>]
//!          [--aops-upstream <url>] [--data <name>=<dir>]...
//!          [--trust-proxy] [--rate-limit <rps>] [--rate-burst <n>]
//!          [--banfile <path>] [--admin-token <secret>]
//!          [--ws-allow <host:port>]... [--allow-private-upstream]
//! ```
//!
//! `--upstream-timeout <secs>` (default 300) bounds how long the proxy waits
//! on a stalled upstream: the connection is aborted if no body bytes arrive
//! for that long (see the client construction in main), so a hung EA CDN /
//! TLS-middlebox upstream cannot pin the handler (and the per-request log
//! line) forever. It is an idle/read-progress budget, not a total request
//! deadline, so legitimately large multi-minute transfers keep flowing while
//! they make progress.
//!
//! The web client boots against this server out of the box: the build default
//! `XUO_WEB_PRODUCT_URL` points at `/patch/manifest/...` (proxied), and shard
//! logins go through `/ws/<host>/<port>`.
//!
//! Internet exposure (all off/loose on the default loopback dev bind):
//!
//! - `--trust-proxy`: take the client IP from X-Forwarded-For (set by the
//!   front proxy, e.g. ferron). Without it the peer socket address is used,
//!   which behind a proxy would rate-limit/ban the proxy's own IP.
//! - `--rate-limit <rps>` + `--rate-burst <n>`: per-IP token bucket, 429s
//!   with Retry-After. `--auto-ban-violations <n>` bans repeat offenders.
//! - `--banfile <path>`: persistent ban list, `<ip[/prefix]> <until> <reason>`
//!   per line, loaded at startup, rewritten on every runtime ban/unban
//!   (write-then-rename). Admin API: `POST /admin/ban?ip=&minutes=&reason=`,
//!   `POST /admin/unban?ip=`, `GET /admin/bans`.
//! - `--admin-token <secret>`: gates `/stats` and `/admin/*` with a Bearer
//!   token (or `?token=`); without it they are disabled (404). `/stats` is a
//!   JSON report: per-lane request/byte counters, status classes, cache
//!   hit/miss, rate-limit rejections, WS bridge gauges (active/opened/
//!   closed/failed, bytes up/down, session durations), ban counts and the
//!   top-10 clients by traffic.
//! - SSRF: on a public bind, `/stream/<url>` and `/blob/...?u=<abs-url>`
//!   refuse private/loopback/link-local upstreams (literal or resolved);
//!   `--allow-private-upstream` restores dev behavior. `/ws/<host>:<port>`
//!   is gated by `--ws-allow <host:port>` (repeatable) when given; without
//!   an allowlist on a public bind it stays open and a startup warning is
//!   printed.
//! - `/healthz` for frontend health checks.

mod admin;
mod normalize;

use std::collections::{HashMap, HashSet};
use std::net::ToSocketAddrs;
use std::path::PathBuf;
use std::sync::atomic::Ordering;
use std::sync::{Arc, Mutex, OnceLock};

use axum::extract::{ws::WebSocket, DefaultBodyLimit, Path, RawQuery, State, WebSocketUpgrade};
use axum::http::{header, HeaderMap, StatusCode};
use axum::response::{IntoResponse, Response};
use axum::routing::get;
use axum::Router;
use clap::Parser;
use futures_util::{SinkExt, StreamExt};
use sha2::{Digest, Sha256};
use tokio::io::{AsyncReadExt, AsyncSeekExt, AsyncWriteExt};

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

// reserved config surface (covers the --root/--cache-dir startup knobs); the
// running server carries its settings in AppState
#[allow(dead_code)]
#[derive(Clone, Debug)]
struct Config {
    listen: String,
    root: PathBuf,
    cache_dir: PathBuf,
    cache_ttl: u64, // seconds; 0 = entries never expire
    // seconds between upstream freshness checks for manifest entries;
    // 0 = revalidate on every request (manifests are KBs, upstreams are CDNs)
    manifest_ttl: u64,
    use_cache: bool,
    upstream_manifest: String,
    upstream_files: String,
    upstream_aops: String,
}

// ---------------------------------------------------------------------------
// entry
// ---------------------------------------------------------------------------

#[derive(clap::Parser)]
#[command(
    name = "xuoproxy",
    about = "caching proxy in front of the UO patch CDN, the web client and local data mounts"
)]
struct Cli {
    /// address:port to listen on
    #[arg(long, default_value = "127.0.0.1:8777")]
    listen: String,

    /// web client (wasm) root tree to serve
    #[arg(long, default_value = "html/")]
    root: String,

    /// on-disk cache directory for fetched upstream content
    #[arg(long, default_value = "xuoproxy-cache")]
    cache_dir: String,

    /// seconds an immutable cached entry stays fresh (0 = forever)
    #[arg(long, default_value_t = 0)]
    cache_ttl: u64,

    /// seconds between upstream freshness checks for manifest entries
    /// (0 = revalidate on every request)
    #[arg(long, default_value_t = 0)]
    manifest_ttl: u64,

    /// seconds of upstream read inactivity (no body progress) before a
    /// fetch is aborted (clamped to >= 1)
    #[arg(long, default_value_t = 300)]
    upstream_timeout: u64,

    /// serve nothing from the on-disk cache (upstream only)
    #[arg(long)]
    no_cache: bool,

    /// upstream manifest repo base url
    #[arg(
        long,
        default_value = "http://patch.uo.broadsword.com/uopatch-sa/legacyrelease/uo/manifest"
    )]
    manifest_upstream: String,

    /// upstream file repo base url
    #[arg(
        long,
        default_value = "http://patch.ak-cdn.eamythic.com/uopatch-sa/250/legacyrelease/uo/files"
    )]
    files_upstream: String,

    /// upstream aops (patchlist/patchcfg) base url
    #[arg(long, default_value = "https://uniteuo.com/")]
    aops_upstream: String,

    /// named data mount: NAME=DIR serves DIR at /data/NAME/... (repeatable);
    /// a bare DIR is the source folder for --generate-product
    #[arg(long = "data", value_name = "NAME=DIR|DIR")]
    data: Vec<String>,

    /// build an unpacked MYP product folder (uo-legacyrelease.prod,
    /// base/pkg.mft, base/unpacked/<name hash>) from a --data folder and
    /// exit; host DIR on any static web server
    #[arg(long, value_name = "DIR", help_heading = "Product generation")]
    generate_product: Option<String>,

    /// filerepo/manifestrepo url prefix baked into the generated product;
    /// set it to the web path the folder will be served at
    #[arg(
        long,
        value_name = "PATH",
        default_value = "/data/uodata/",
        help_heading = "Product generation"
    )]
    generate_url: String,

    /// honor X-Forwarded-For (behind ferron/nginx)
    #[arg(long, help_heading = "Internet exposure")]
    trust_proxy: bool,

    /// per-IP requests/sec (0 = off)
    #[arg(long, default_value_t = 0, help_heading = "Internet exposure")]
    rate_limit: u64,

    /// per-IP burst above the rate
    #[arg(long, default_value_t = 30, help_heading = "Internet exposure")]
    rate_burst: u32,

    /// ban IPs after N rate-limit hits (0 = off)
    #[arg(long, default_value_t = 0, help_heading = "Internet exposure")]
    auto_ban_violations: u32,

    /// persistent ban list (ip[/prefix] until reason)
    #[arg(long, value_name = "PATH", help_heading = "Internet exposure")]
    banfile: Option<PathBuf>,

    /// enable /stats + /admin (Bearer token)
    #[arg(long, value_name = "SECRET", help_heading = "Internet exposure")]
    admin_token: Option<String>,

    /// restrict /ws bridges to host:port (repeatable; default open)
    #[arg(long, value_name = "HOST:PORT", help_heading = "Internet exposure")]
    ws_allow: Vec<String>,

    /// permit /stream,/blob to fetch private addresses
    #[arg(long, help_heading = "Internet exposure")]
    allow_private_upstream: bool,
}

#[tokio::main]
async fn main() {
    let cli = Cli::parse();

    let listen = cli.listen;
    let root = cli.root;
    let cache_dir = cli.cache_dir;
    let cache_ttl = cli.cache_ttl;
    let manifest_ttl = cli.manifest_ttl;
    // clamp to >= 1s: 0 would disable reqwest's read timeout entirely,
    // turning the footgun into stuck upstreams that hang a request (and its
    // log line) forever
    let upstream_timeout = cli.upstream_timeout.max(1);
    let use_cache = !cli.no_cache;
    let upstream_manifest = cli.manifest_upstream;
    let upstream_files = cli.files_upstream;
    let upstream_aops = cli.aops_upstream;
    let data_dirs: Arc<Mutex<HashMap<String, PathBuf>>> = Arc::new(Mutex::new(HashMap::new()));

    let trust_proxy = cli.trust_proxy;
    let rate_limit = cli.rate_limit;
    let rate_burst = cli.rate_burst.max(1);
    let auto_ban_violations = cli.auto_ban_violations;
    let admin_token = cli.admin_token;
    let allow_private_upstream = cli.allow_private_upstream;
    let banfile = cli.banfile;
    let mut ws_allow: Option<HashSet<String>> = if cli.ws_allow.is_empty() {
        None
    } else {
        Some(
            cli.ws_allow
                .iter()
                .map(|v| v.trim().to_lowercase())
                .collect(),
        )
    };
    let generate_product = cli.generate_product;
    let generate_url = cli.generate_url;
    // repeatable named mount: --data <name>=<dir> serves <dir> at
    // /data/<name>/... (a shard's local install; shards.cfg points its
    // producturl= at the mount name); a bare <dir> is only meaningful with
    // --generate-product
    let mut generate_source: Option<PathBuf> = None;
    for data in &cli.data {
        let Some((name, dir)) = data.split_once('=') else {
            generate_source = Some(PathBuf::from(data.trim()));
            continue;
        };
        let name = name.trim();
        if name.is_empty()
            || !name
                .bytes()
                .all(|b| b.is_ascii_alphanumeric() || b == b'_' || b == b'-')
        {
            eprintln!("--data mount name must be [A-Za-z0-9_-]+, got: {name}");
            std::process::exit(2);
        }
        if data_dirs
            .lock()
            .expect("data mounts")
            .insert(name.to_string(), PathBuf::from(dir.trim()))
            .is_some()
        {
            eprintln!("--data mount redefined: {name}");
            std::process::exit(2);
        }
    }

    // --generate-product: offline mode - build the product folder and exit,
    // before any server state (binds, cache) is set up
    if let Some(out_dir) = generate_product {
        let Some(src) = generate_source else {
            eprintln!("--generate-product needs a source folder: --data <dir>");
            std::process::exit(2);
        };
        match generate_product_folder(&src, std::path::Path::new(&out_dir), &generate_url).await {
            Ok((serial, files)) => {
                println!(
                    "xuoproxy: product written to {out_dir} (serial {serial:016x}, {files} files, repo url {generate_url})"
                );
            }
            Err(e) => {
                eprintln!("xuoproxy: --generate-product failed: {e}");
                std::process::exit(1);
            }
        }
        std::process::exit(0);
    }

    // auto-derive the ws allowlist from the root shards.cfg: every
    // LoginServer= address a shard can hand the client becomes bridgeable
    // (explicit --ws-allow entries are kept as additional entries, shown
    // as source "cli"). Read before serving starts; a changed shards.cfg
    // needs a restart. Empty result + no --ws-allow = open bridges.
    let mut ws_allow_info: Vec<(String, String, u16)> = Vec::new();
    if let Ok(text) = std::fs::read_to_string(PathBuf::from(&root).join("shards.cfg")) {
        for (shard, servers) in shards_login_servers(&text) {
            for (host, port) in servers {
                ws_allow
                    .get_or_insert_with(HashSet::new)
                    .insert(format!("{host}:{port}"));
                ws_allow_info.push((shard.clone(), host, port));
            }
        }
    } else if ws_allow.is_none() {
        println!("xuoproxy: no shards.cfg in {root} and no --ws-allow: /ws bridges are OPEN");
    }

    // shards commonly redirect the client at the game server as a bare IP
    // inside the 0xA8/0x8C relay reply even when shards.cfg only lists dns
    // names: pre-resolve every dns entry in the allowlist and allow the IP
    // forms too, or the reconnect would be refused by ws_allowed(). the
    // resolved IPs are added bare (no port): the allowlist matches a
    // host-only key against any port, mirroring how the dns names
    // themselves behave (a "host:port" entry also allows every port on
    // host). done once at startup (blocking std resolution, before serving
    // starts); like shards.cfg itself, a changed dns answer needs a
    // restart. IP-literal entries resolve to themselves and are no-ops.
    if let Some(set) = ws_allow.as_mut() {
        let mut resolved: Vec<(String, String, u16)> = Vec::new(); // (ip, source, port)
        for entry in set.iter() {
            let Ok(addrs) = entry.to_socket_addrs() else {
                eprintln!("xuoproxy: ws-allow: cannot resolve \"{entry}\" (kept as-is)");
                continue;
            };
            for addr in addrs {
                let ip = addr.ip().to_string();
                let port = addr.port();
                if !set.contains(&ip) {
                    resolved.push((ip, entry.clone(), port));
                }
            }
        }
        for (ip, source, port) in &resolved {
            set.insert(ip.clone());
            ws_allow_info.push(("dns".to_string(), ip.clone(), *port));
            println!("xuoproxy: ws-allow: resolved {source} -> {ip} (any port, first seen :{port})");
        }
    }

    let admin = Arc::new(admin::Admin::new(
        trust_proxy,
        rate_limit,
        rate_burst,
        auto_ban_violations,
        admin_token,
        allow_private_upstream,
        ws_allow,
        banfile,
    ));

    let state = AppState {
        root: PathBuf::from(&root),
        cache_dir: PathBuf::from(&cache_dir),
        cache_ttl,
        manifest_ttl,
        use_cache,
        upstream_manifest: upstream_manifest.trim_end_matches('/').to_string(),
        upstream_files: upstream_files.trim_end_matches('/').to_string(),
        upstream_aops: upstream_aops.trim_end_matches('/').to_string(),
        upstream_timeout,
        data_dirs: data_dirs.clone(),
        http: reqwest::Client::builder()
            .connect_timeout(std::time::Duration::from_secs(30))
            // idle/read-progress budget for upstream connections: this is
            // reqwest's per-read timeout (it resets after every successful
            // body frame), NOT a total request deadline. It bounds a stuck
            // handler - and therefore the per-request log line - when an
            // upstream accepts the connection but stops sending (EA CDN
            // hiccups, TLS-intercepting middleboxes, slow mirrors), while a
            // legitimately huge transfer keeps flowing as long as it makes
            // progress. A total `.timeout()` was rejected: the relay routes
            // (/stream, /blob, generic) stream multi-GB bodies through this
            // same client, and a fixed total deadline would abort legitimate
            // long transfers mid-stream. The web client's own retry/backoff
            // masks brief stalls far faster than this budget.
            .read_timeout(std::time::Duration::from_secs(upstream_timeout))
            .build()
            .expect("http client"),
        admin: admin.clone(),
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
        .route("/blob/:hash/:offset/:size", get(blob_window))
        .route("/blob/:offset/:size", get(blob_window_open))
        .route("/stream/*url", get(stream_proxy))
        .route("/ws/:host/:port", get(ws_shard))
        .route("/data/:name/*path", get(data_file))
        .route("/healthz", get(|| async { "ok" }))
        .route("/stats", get(admin::stats_handler))
        .route("/admin/bans", get(admin::bans_handler))
        .route("/admin/ban", axum::routing::post(admin::ban_handler))
        .route("/admin/unban", axum::routing::post(admin::unban_handler))
        .fallback(static_file)
        .layer(axum::middleware::from_fn(coop_coep))
        .layer(axum::middleware::from_fn(log_requests))
        // outermost first-served: bans/rate-limit before stats so rejected
        // requests still count, stats wraps the body so served bytes are real
        .layer(axum::middleware::from_fn_with_state(
            state.clone(),
            admin::stats_mw,
        ))
        .layer(axum::middleware::from_fn_with_state(
            state.clone(),
            admin::guard,
        ))
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
    println!("  static: {root}");
    println!(
        "  proxy:  /patch/manifest|files|eamythic + /stream/<url> + /blob/[<hash>/]<off>/<len> (cache: {})",
        cache_dir
    );
    println!("  upstream: read-timeout {upstream_timeout}s (idle/read-progress)");
    println!("  bridge: /ws/<host>/<port>");
    {
        // the effective allowlist, with the shard each entry came from
        let info = &ws_allow_info;
        if info.is_empty() {
            println!("  Allowed: (open - ALL hosts; pass --ws-allow or ship a shards.cfg)");
        } else {
            println!("  Allowed:");
            for (shard, host, port) in info.iter() {
                println!("  - {shard}: {host}:{port}");
            }
        }
    }
    if state.admin.ws_allow.is_none() && listen_start_public(&listen) {
        println!(
            "    WARNING: bound publicly with NO --ws-allow allowlist; /ws can reach\n             ANY host:port from this machine (TCP SSRF). Restrict it for production."
        );
    }
    if listen_start_public(&listen) && !state.admin.allow_private_upstream {
        println!("  ssrf:   private upstreams DENIED on /stream + /blob u= (dev on loopback, or --allow-private-upstream)");
    }
    println!(
        "  admin:  {}",
        if state.admin.admin_token.is_some() {
            "/stats + /admin/ban|unban|bans (token required)"
        } else {
            "disabled (no --admin-token)"
        }
    );
    println!(
        "  guard:  rate-limit {} rps (burst {}), trust-proxy {}, auto-ban after {} violations",
        state.admin.rate_rps,
        state.admin.rate_burst,
        state.admin.trust_proxy,
        state.admin.auto_ban_violations
    );
    {
        let mounts = data_dirs.lock().expect("data mounts");
        if mounts.is_empty() {
            println!("  data:   no local mounts (--data <name>=<dir>; shards.cfg ProductUrl=local:<path> auto-mounts too)");
        } else {
            let mut names: Vec<_> = mounts.iter().collect();
            names.sort_by(|a, b| a.0.cmp(b.0));
            println!(
                "  data:   /data/<name>/ with {} mount(s): {}",
                names.len(),
                names
                    .iter()
                    .map(|(n, d)| format!("{n} -> {}", d.display()))
                    .collect::<Vec<_>>()
                    .join(", ")
            );
        }
    }
    println!(
        "  fresh:  manifests revalidate upstream every request (manifest-ttl {}s; conditional GET + content invalidation on change)",
        manifest_ttl
    );
    let listener = tokio::net::TcpListener::bind(&listen).await.expect("bind");
    axum::serve(
        listener,
        app.into_make_service_with_connect_info::<std::net::SocketAddr>(),
    )
    .await
    .expect("serve");
}

// the listener is internet-reachable unless it binds a loopback address
fn listen_start_public(listen: &str) -> bool {
    let host = listen.rsplit_once(':').map(|(h, _)| h).unwrap_or(listen);
    match host.parse::<std::net::IpAddr>() {
        Ok(ip) => !ip.is_loopback(), // 0.0.0.0 binds all interfaces: public
        Err(_) => true,              // hostnames: assume public
    }
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
    let prod_url = match state.upstream_manifest.rsplit_once('/') {
        // both arms resolve to the product url the client requests
        Some((_, _last)) => format!("{}/uo-legacyrelease.prod", state.upstream_manifest),
        _ => format!("{}/uo-legacyrelease.prod", state.upstream_manifest),
    };
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
    root: PathBuf,
    cache_dir: PathBuf,
    cache_ttl: u64,
    manifest_ttl: u64,
    use_cache: bool,
    upstream_manifest: String,
    upstream_files: String,
    upstream_aops: String,
    upstream_timeout: u64,
    // local data mounts (/data/<name>/...): explicit --data mounts plus the
    // shards.cfg `ProductUrl=local:<path>` auto-mounts (shared because the
    // registry grows lazily when shards.cfg is served or an unknown mount is
    // resolved)
    data_dirs: Arc<Mutex<HashMap<String, PathBuf>>>,
    http: reqwest::Client,
    admin: Arc<admin::Admin>,
}

// reqwest's Display for a body-read failure is just "error decoding
// response body", which reads like a proxy bug; name the real cause when
// the error is a timeout (the client builder's read_timeout is the budget).
// Status codes stay as chosen by the caller.
fn upstream_error_body(state: &AppState, kind: &str, e: &reqwest::Error) -> String {
    if e.is_timeout() {
        format!(
            "upstream timeout after {}s ({} stalled)",
            state.upstream_timeout, kind
        )
    } else {
        format!("upstream {kind} failed: {e}")
    }
}

// one-line request log (stdout): boot-flow visibility and run-to-run request
// counts (blob-window route hits vs ranged 206 answers, first vs warm boots)
async fn log_requests(req: axum::extract::Request, next: axum::middleware::Next) -> Response {
    let method = req.method().clone();
    let path = req
        .uri()
        .path_and_query()
        .map(|pq| pq.as_str().to_string())
        .unwrap_or_else(|| req.uri().path().to_string());
    // percent-encoded ?u= values get long; keep one line readable
    let shown: String = if path.chars().count() > 160 {
        path.chars().take(160).collect::<String>() + "..."
    } else {
        path
    };
    let started = std::time::Instant::now();
    let mut res = next.run(req).await;
    println!(
        "xuoproxy: {method} {shown} -> {} ({} bytes, {} us)",
        res.status().as_u16(),
        {
            use axum::body::HttpBody;
            res.body_mut().size_hint().exact().unwrap_or(0)
        },
        started.elapsed().as_micros()
    );
    res
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
                        upstream_error_body(state, "read", &e),
                    )
                        .into_response()
                }
            },
            Err(e) => {
                return (
                    StatusCode::BAD_GATEWAY,
                    upstream_error_body(state, "fetch", &e),
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
            // manifest-flavored entries (aops patchlists/patchcfgs fetched
            // through this lane) revalidate upstream before serving - see
            // manifest_entry_serve
            let meta = cache_meta(state, &url).unwrap_or_default();
            if meta.manifest && status == 200 {
                return manifest_entry_serve(
                    state,
                    &url,
                    status,
                    &sanitize_ctype(&ctype),
                    body,
                    &meta,
                    async |raw, etag, lm| {
                        generic_manifest_process(state, &url, raw, etag, lm).await
                    },
                    |status, ctype, body| {
                        respond_text_rewritten(state, status, ctype, body, headers, None, true)
                    },
                )
                .await;
            }
            // ranged request against a cached full body: slice + 206 (the
            // cache only stores full 200 bodies; without this a cached entry
            // would shadow every range read - the web chunked fetch and the
            // block streaming both depend on real 206 answers)
            let etag = strong_etag(&body);
            if let Some(resp) = range_from_cached(&sanitize_ctype(&ctype), &body, headers, &etag) {
                return resp;
            }
            return respond_text_rewritten(
                state,
                status,
                &sanitize_ctype(&ctype),
                body,
                headers,
                None,
                meta.manifest,
            );
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
                upstream_error_body(state, "fetch", &e),
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
    let upstream_lm = resp
        .headers()
        .get(header::LAST_MODIFIED)
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
                upstream_error_body(state, "read", &e),
            )
                .into_response()
        }
    };

    let mut manifest = false;
    if state.use_cache && status == 200 {
        let (_, meta) = generic_manifest_process(
            state,
            &url,
            body.clone(),
            upstream_etag.clone(),
            upstream_lm,
        )
        .await;
        manifest = meta.manifest;
        cache_put_with_meta(state, &url, status, &sanitize_ctype(&ctype), &body, &meta);
    }

    respond_text_rewritten(
        state,
        status,
        &sanitize_ctype(&ctype),
        body,
        headers,
        upstream_etag.as_deref(),
        manifest,
    )
}

fn cache_entry(state: &AppState, url: &str) -> PathBuf {
    state
        .cache_dir
        .join(format!("{}.cache", sha256_hex(url.as_bytes())))
}

// cache entry v2/v3 layout:
//   [status:2 BE][etag_len:4 BE][ctype_len:4 BE][etag][ctype][body][meta][meta_len:4 BE]
// 10-byte fixed prefix (status + the two length fields); the etag is stored
// WITH its quotes (exactly as strong_etag produces it), so the whole-body 200
// and the windowed 206 answer the same URL with an identical validator without
// the 206 path reading (or hashing) the body.
//
// v3 appends the metadata tail (`meta` = CacheMeta, see cache_meta_encode),
// terminated by its own 4-byte big-endian length.
// Version tolerance: a tail is recognized ONLY when meta_len is in
// [MAGIC.len(), META_LEN_MAX] and the meta bytes carry the "cv=1" magic that
// every encoded meta starts with; anything else - in particular every
// tail-less v2 entry - parses as body-only with CacheMeta::default(), i.e.
// "no validators, treat as fresh": old caches keep working, one rewrite turns
// each entry v3.
const META_LEN_MAX: usize = 64 * 1024;
const META_MAGIC: &[u8] = b"cv=1";

#[derive(Clone, Default)]
struct CacheMeta {
    // upstream validators captured when the body was fetched (conditional
    // GET material for the revalidation pass)
    upstream_etag: Option<String>,
    upstream_last_modified: Option<String>,
    // unix secs of the last upstream check (the --manifest-ttl throttle)
    checked_at: Option<u64>,
    // manifest-flavored body: revalidate upstream before serving
    manifest: bool,
    // patchcfg entries: resolved upstream data root + [project]
    // custom_extension (the patchlist reference set is derived from them)
    patch_directory: Option<String>,
    custom_extension: Option<String>,
    // upstream urls (disk-cache keys) this manifest body referenced
    references: Vec<String>,
}

// line-based key=value text, version-tolerant: unknown keys are skipped, a
// parse failure degrades to the default meta ("no validators")
fn cache_meta_encode(meta: &CacheMeta) -> Vec<u8> {
    let mut out =
        String::with_capacity(64 + meta.references.iter().map(|u| u.len() + 4).sum::<usize>());
    out.push_str("cv=1\n");
    if let Some(etag) = &meta.upstream_etag {
        out.push_str(&format!("et={etag}\n"));
    }
    if let Some(lm) = &meta.upstream_last_modified {
        out.push_str(&format!("lm={lm}\n"));
    }
    if let Some(at) = meta.checked_at {
        out.push_str(&format!("ck={at}\n"));
    }
    if meta.manifest {
        out.push_str("mf=1\n");
    }
    if let Some(pd) = &meta.patch_directory {
        out.push_str(&format!("pd={pd}\n"));
    }
    if let Some(ce) = &meta.custom_extension {
        out.push_str(&format!("ce={ce}\n"));
    }
    for url in &meta.references {
        out.push_str(&format!("rf={url}\n"));
    }
    out.into_bytes()
}

fn cache_meta_decode(bytes: &[u8]) -> CacheMeta {
    let mut meta = CacheMeta::default();
    let text = String::from_utf8_lossy(bytes);
    for line in text.lines() {
        let Some((key, value)) = line.split_once('=') else {
            continue;
        };
        match key {
            "et" => meta.upstream_etag = Some(value.to_string()),
            "lm" => meta.upstream_last_modified = Some(value.to_string()),
            "ck" => meta.checked_at = value.parse().ok(),
            "mf" => meta.manifest = value == "1",
            "pd" => meta.patch_directory = Some(value.to_string()),
            "ce" => meta.custom_extension = Some(value.to_string()),
            "rf" => meta.references.push(value.to_string()),
            _ => {} // future keys pass through untouched
        }
    }
    meta
}

// the stored metadata tail of one entry, without reading the body; a tail-
// less (v2) or malformed entry yields the default meta ("no validators")
fn cache_meta(state: &AppState, url: &str) -> Option<CacheMeta> {
    let raw = std::fs::read(cache_entry(state, url)).ok()?;
    let (meta, _) = split_cache_tail(&raw)?;
    Some(meta)
}

// splits [..body][meta][meta_len:4 BE] off a raw entry read; returns the
// default meta for tail-less v2 entries (see the layout comment above). The
// magic check keeps v2 bodies whose last bytes merely look like a small
// length parsing as body-only.
fn split_cache_tail(raw: &[u8]) -> Option<(CacheMeta, &[u8])> {
    if raw.len() < 4 {
        return None;
    }
    let ml = u32::from_be_bytes([
        raw[raw.len() - 4],
        raw[raw.len() - 3],
        raw[raw.len() - 2],
        raw[raw.len() - 1],
    ]) as usize;
    let rest = &raw[..raw.len() - 4];
    if ml >= META_MAGIC.len() && ml <= META_LEN_MAX && rest.len() >= ml {
        let meta_bytes = &rest[rest.len() - ml..];
        if meta_bytes.starts_with(META_MAGIC) {
            return Some((cache_meta_decode(meta_bytes), &rest[..rest.len() - ml]));
        }
    }
    // not a v3 tail: the last four bytes are body bytes (v2 entry)
    Some((CacheMeta::default(), raw))
}
fn cache_get(state: &AppState, url: &str) -> Option<(u16, String, String, Vec<u8>)> {
    let path = cache_entry(state, url);
    let raw = match std::fs::read(&path) {
        Ok(raw) => raw,
        Err(_) => {
            state.admin.stats.cache_miss();
            return None;
        }
    };

    if !cache_fresh(state, &path) {
        state.admin.stats.cache_miss();
        return None;
    }

    // strip the v3 metadata tail first (a no-op slice for v2 entries)
    let (_, body_and_head) = split_cache_tail(&raw)?;
    let raw = body_and_head;

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
        state.admin.stats.cache_miss();
        return None;
    }
    let etag = String::from_utf8_lossy(&raw[10..10 + etag_len]).into_owned();
    let bo = 10 + etag_len;
    if raw.len() < bo + ctype_len {
        state.admin.stats.cache_miss();
        return None;
    }
    let ctype = String::from_utf8_lossy(&raw[bo..bo + ctype_len]).into_owned();
    let body = raw[bo + ctype_len..].to_vec();
    state.admin.stats.cache_hit();
    Some((status, ctype, etag, body))
}

fn cache_put(state: &AppState, url: &str, status: u16, ctype: &str, body: &[u8]) {
    cache_put_with_meta(state, url, status, ctype, body, &CacheMeta::default());
}

fn cache_put_with_meta(
    state: &AppState,
    url: &str,
    status: u16,
    ctype: &str,
    body: &[u8],
    meta: &CacheMeta,
) {
    let path = cache_entry(state, url);
    let etag = strong_etag(body);
    let meta_bytes = cache_meta_encode(meta);
    println!(
        "cache_put {} ({} bytes, {}, {}, meta {} bytes)",
        path.display(),
        body.len(),
        ctype,
        etag,
        meta_bytes.len()
    );
    let _ = std::fs::create_dir_all(&state.cache_dir);
    let mut out =
        Vec::with_capacity(10 + etag.len() + ctype.len() + body.len() + 4 + meta_bytes.len());
    out.extend_from_slice(&status.to_be_bytes());
    out.extend_from_slice(&(etag.len() as u32).to_be_bytes());
    out.extend_from_slice(&(ctype.len() as u32).to_be_bytes());
    out.extend_from_slice(etag.as_bytes());
    out.extend_from_slice(ctype.as_bytes());
    out.extend_from_slice(body);
    out.extend_from_slice(&meta_bytes);
    // the length TRAILS the meta: a reader only needs the file's last four
    // bytes to locate the tail (no second seek), which is what keeps the
    // windowed reads tail-aware without a full read
    out.extend_from_slice(&(meta_bytes.len() as u32).to_be_bytes());
    let _ = std::fs::write(&path, &out);
}

// drops one disk entry (targeted content invalidation: a changed manifest's
// old referenced urls re-fetch fresh on their next request). A missing entry
// is not an error - nothing to invalidate.
fn cache_invalidate(state: &AppState, url: &str) -> bool {
    if std::fs::remove_file(cache_entry(state, url)).is_ok() {
        println!("xuoproxy: cache invalidated {url}");
        true
    } else {
        false
    }
}

// targeted content invalidation: the disk entries the OLD manifest body
// referenced are dropped when that body changed upstream, so every
// referenced url re-fetches fresh on its next request (the NEW body's
// reference set is persisted with the new entry for the next round)
fn invalidate_references(state: &AppState, old_refs: &[String]) {
    for url in old_refs {
        cache_invalidate(state, url);
    }
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

// short content fingerprint for browser-facing version tokens: stable across
// publishes for unchanged resources (no churn), different the moment the
// bytes or their upstream validator move
fn short_token(data: &[u8]) -> String {
    sha256_hex(data)[..16].to_string()
}

fn now_secs() -> u64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(0)
}

// --manifest-ttl throttle: how long a manifest entry trusts its last upstream
// check. 0 = every request revalidates (the default: manifests are KBs and
// upstreams are CDNs).
fn manifest_revalidate_due(state: &AppState, meta: &CacheMeta) -> bool {
    if state.manifest_ttl == 0 {
        return true;
    }
    match meta.checked_at {
        Some(at) => now_secs().saturating_sub(at) >= state.manifest_ttl,
        None => true,
    }
}

// re-persist an entry with only its last-checked timestamp bumped (the 304
// path: the body did not change, the throttle window restarts)
fn cache_touch(
    state: &AppState,
    url: &str,
    status: u16,
    ctype: &str,
    body: &[u8],
    meta: &CacheMeta,
) {
    let mut meta = meta.clone();
    meta.checked_at = Some(now_secs());
    cache_put_with_meta(state, url, status, ctype, body, &meta);
}

// outcome of the upstream freshness check for a cached manifest entry
enum UpstreamCheck {
    // upstream validators still match: the cached copy is current
    NotModified,
    // upstream sent a fresh body (response not yet consumed)
    Modified(reqwest::Response),
    // upstream answered with an error / was unreachable: stale serving keeps
    // the client booting (availability over freshness)
    Other(u16),
    Failed(String),
}

// conditional GET against the validators captured at cache time; with no
// stored validators this degrades to a plain GET (the 200 path re-captures
// them, so the next check is conditional again)
async fn conditional_get(state: &AppState, url: &str, meta: &CacheMeta) -> UpstreamCheck {
    let mut req = state.http.get(url);
    if let Some(etag) = &meta.upstream_etag {
        req = req.header(header::IF_NONE_MATCH, etag.clone());
    }
    if let Some(lm) = &meta.upstream_last_modified {
        req = req.header(header::IF_MODIFIED_SINCE, lm.clone());
    }
    println!(
        "xuoproxy: revalidating {url} upstream ({}{})",
        meta.upstream_etag.as_deref().unwrap_or("no etag"),
        meta.upstream_last_modified
            .as_deref()
            .map(|lm| format!(", if-modified-since {lm}"))
            .unwrap_or_default()
    );
    match req.send().await {
        Ok(resp) => {
            let status = resp.status().as_u16();
            if status == 304 {
                UpstreamCheck::NotModified
            } else if resp.status().is_success() {
                UpstreamCheck::Modified(resp)
            } else {
                UpstreamCheck::Other(status)
            }
        }
        Err(e) => UpstreamCheck::Failed(format!("{e}")),
    }
}

// upstream validators of a fresh (Modified) response
fn response_validators(resp: &reqwest::Response) -> (Option<String>, Option<String>) {
    let get = |name: &str| {
        resp.headers()
            .get(name)
            .and_then(|v| v.to_str().ok())
            .map(str::to_string)
            .filter(|v| !v.is_empty())
    };
    (get("etag"), get("last-modified"))
}

// THE LINCHPIN of the browser-facing cache-busting tokens: rewritten resource
// urls carry `?v=<token>` (see rewrite_links_tokened). The token is
// browser-facing ONLY - it must never reach an upstream url or a disk-cache
// key, or the same resource would fork into per-publish cache entries. Every
// incoming url resolution strips it before anything is resolved or hashed.
//
// Ordering contract (percent-decode vs parameter split): the `u=` value of
// the /blob routes is located on the RAW, still-encoded query (an encoded
// value cannot contain a literal '&'), percent-decoded AFTER the split, and
// the token strip runs on the DECODED url - so a `?v=` that traveled inside
// an encoded `u` is seen here exactly once, in plain form.
//
// Handles both placements this proxy emits or a client join can produce:
//   ".../file?v=<token>"          - token appended to a full resource url
//   ".../dir?v=<token>/tail..."   - token on a repo root the client joined a
//                                   tail onto (the '/' bounds the token)
fn strip_version_param(url: &str) -> String {
    let Some(at) = url.find("?v=").or_else(|| url.find("&v=")) else {
        return url.to_string();
    };
    let value_start = at + 3;
    let value_end = url[value_start..]
        .find(['&', '/'])
        .map(|i| value_start + i)
        .unwrap_or(url.len());
    let mut out = String::with_capacity(url.len());
    out.push_str(&url[..at]);
    if url[at..].starts_with('?') && url[value_end..].starts_with('&') {
        // "?v=x&rest=..." keeps a valid query: splice out the param, keep "?"
        out.push('?');
        out.push_str(&url[value_end + 1..]);
    } else {
        out.push_str(&url[value_end..]);
    }
    out
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
    let mut res = (status, [(header::CONTENT_TYPE, ctype.to_string())], body).into_response();
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
    state: &AppState,
    status: u16,
    ctype: &str,
    body: Vec<u8>,
    headers: &HeaderMap,
    upstream_etag: Option<&str>,
    manifest: bool,
) -> Response {
    let status = StatusCode::from_u16(status).unwrap_or(StatusCode::INTERNAL_SERVER_ERROR);
    let rewritten = rewrite_links(state, &body, headers);
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
    // generic-route payloads: content bodies are versioned-repo artifacts the
    // disk cache already pins forever (cache_ttl 0): the same immutable +
    // content-hash treatment as /patch/files lets browsers reuse them, while
    // the ETag still answers explicit conditional GETs with a cheap 304;
    // manifest-flavored entries (aops patchlists/patchcfgs) can change
    // upstream, so they revalidate like every other manifest lane
    cacheable_response(
        status,
        ctype,
        body,
        headers,
        &etag,
        if manifest {
            CACHE_REVALIDATE
        } else {
            CACHE_IMMUTABLE
        },
    )
}

// ---------------------------------------------------------------------------
// manifest route: serves MythicMFT xml and normalizes whole-file .uop
// transfers for the web client
// ---------------------------------------------------------------------------

// the manifest pipeline shared by the fresh fetch and the revalidated fetch
// so both produce byte-identical bodies: inflate ct=1 transfers, rewrite
// upstream urls (with browser-facing version tokens, whose manifest-level
// fallback is the upstream validator / body hash), normalize every
// whole-file .uop transfer this proxy serves plain and declare the
// normalized sizes. Returns the final body plus the upstream url reference
// set (for targeted content invalidation on change) and whether the body was
// actually processed as a manifest (vs passed through as-is).
async fn build_manifest_body(
    state: &AppState,
    tail: &str,
    raw: Vec<u8>,
    fallback_token: &str,
) -> (Vec<u8>, Vec<String>, bool) {
    let compressed = normalize::looks_zlib(&raw);
    let mut plain = raw.clone();
    if compressed {
        plain = normalize::zlib_inflate(&raw).unwrap_or(raw.clone());
    }
    if let Ok(text) = std::str::from_utf8(&plain) {
        if text.trim_start().starts_with('<') {
            let (rewritten, references) =
                rewrite_links_tokened(state, &plain, &HeaderMap::new(), Some(fallback_token));
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
            let body = if compressed {
                normalize::zlib_deflate(&rewritten_text)
            } else {
                rewritten_text
            };
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
            return (body, references, true);
        }
    }
    // not a text manifest: passthrough exactly as shipped
    (plain, Vec::new(), false)
}

fn manifest_meta(etag: Option<String>, lm: Option<String>, references: Vec<String>) -> CacheMeta {
    CacheMeta {
        upstream_etag: etag,
        upstream_last_modified: lm,
        checked_at: Some(now_secs()),
        manifest: true,
        patch_directory: None,
        custom_extension: None,
        references,
    }
}

// the 200 + no-cache + strong-etag answer every manifest-route path serves
// from a cached body (identical validator whichever path produced it)
fn manifest_cached_answer(body: Vec<u8>, req_headers: &HeaderMap) -> Response {
    let etag = strong_etag(&body);
    cacheable_response(
        StatusCode::OK,
        "text/xml",
        body,
        req_headers,
        &etag,
        CACHE_REVALIDATE,
    )
}

async fn manifest_response(state: &AppState, tail: &str, req_headers: &HeaderMap) -> Response {
    let url = format!("{}/{tail}", state.upstream_manifest);

    // phase 0 freshness: a cached manifest is NEVER served blind. Either the
    // --manifest-ttl throttle still trusts its last upstream check, or a
    // conditional GET with the stored validators runs first.
    if state.use_cache {
        if let Some((ctype, body)) = cached_file(state, &url) {
            let meta = cache_meta(state, &url).unwrap_or_default();
            return manifest_entry_serve(
                state,
                &url,
                200,
                &sanitize_ctype(&ctype),
                body,
                &meta,
                async |raw, etag, lm| {
                    let fallback = match &etag {
                        Some(etag) => short_token(etag.as_bytes()),
                        None => short_token(&raw),
                    };
                    let (body, references, _) =
                        build_manifest_body(state, tail, raw, &fallback).await;
                    (body, manifest_meta(etag, lm, references))
                },
                |status, ctype, body| match status {
                    200 => manifest_cached_answer(body, req_headers),
                    _ => respond_plain(status, ctype, body),
                },
            )
            .await;
        }
    }

    let resp = match state.http.get(&url).send().await {
        Ok(resp) => resp,
        Err(e) => {
            return (
                StatusCode::BAD_GATEWAY,
                upstream_error_body(state, "fetch", &e),
            )
                .into_response()
        }
    };
    let status = resp.status().as_u16();
    let (upstream_etag, upstream_lm) = response_validators(&resp);
    let raw = match resp.bytes().await {
        Ok(b) => b.to_vec(),
        Err(e) => {
            return (
                StatusCode::BAD_GATEWAY,
                upstream_error_body(state, "read", &e),
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
    let fallback_token = match &upstream_etag {
        Some(etag) => short_token(etag.as_bytes()),
        None => short_token(&raw),
    };
    let (body, references, processed) =
        build_manifest_body(state, tail, raw, &fallback_token).await;
    let status = if processed { 200 } else { status };

    if state.use_cache && status == 200 {
        cache_put_with_meta(
            state,
            &url,
            status,
            "text/xml",
            &body,
            &manifest_meta(upstream_etag, upstream_lm, references),
        );
    }
    let served_bytes = body.len();
    // manifests change with every publish: never let the browser trust its
    // copy blindly (no-cache), but hand it the content hash so an unchanged
    // revalidation is a cheap 304 instead of a full refetch
    let res = if status == 200 {
        manifest_cached_answer(body, req_headers)
    } else {
        let mut res =
            (StatusCode::from_u16(status).unwrap_or(StatusCode::OK), body).into_response();
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
        .map_err(|e| upstream_error_body(state, "fetch", &e))?;
    if !resp.status().is_success() {
        return Err(format!("upstream status {}", resp.status().as_u16()));
    }
    let raw = resp
        .bytes()
        .await
        .map_err(|e| upstream_error_body(state, "read", &e))?
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
    // 10-byte fixed prefix: status(2) + etag_len(4) + ctype_len(4); the v3
    // metadata tail (4-byte length + meta) is chopped off the body length
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
    // the tail is at the very end: [meta][meta_len:4 BE]; the magic-bound
    // check mirrors split_cache_tail (a v2 entry tail-parses as absent)
    let mut tail_len_buf = [0u8; 4];
    if file_len >= offset + 4 {
        use std::io::{Seek, SeekFrom};
        f.seek(SeekFrom::End(-4)).ok()?;
        std::io::Read::read_exact(&mut f, &mut tail_len_buf).ok()?;
    }
    let ml = u32::from_be_bytes(tail_len_buf) as usize;
    let tail = if ml >= META_MAGIC.len() && ml <= META_LEN_MAX && file_len >= offset + 4 + ml {
        4 + ml
    } else {
        0
    };
    let body_len = (file_len - offset - tail) as u64;
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
// /blob/<urlhash>/<offset>/<size>?u=<url>: whole-blob window route
//
// Ranged reads as plain cacheable 200s instead of one-shot 206s: exactly
// `size` bytes of the upstream resource starting at `offset` (clamped at
// EOF), immutable freshness + a strong slice ETag, so every intermediate
// cache (browser, HTTP, this proxy's own disk) can store and reuse the
// window like a whole file. Never answers 206 and never forwards a Range
// upstream - the proxy fetches the whole body once and ranges itself.
// ---------------------------------------------------------------------------

// --cache-ttl freshness (0 = entries live forever). Unreadable metadata
// defers to the read itself, exactly like the cache_get fast path.
fn cache_fresh(state: &AppState, path: &std::path::Path) -> bool {
    if state.cache_ttl == 0 {
        return true;
    }
    let Ok(meta) = std::fs::metadata(path) else {
        return true;
    };
    let Ok(modified) = meta.modified() else {
        return true;
    };
    let Ok(age) = modified.elapsed() else {
        return true;
    };
    age.as_secs() <= state.cache_ttl
}

// resource length of a cached entry without reading the body: the v2 prefix
// is fixed-width, so total = file size - 10 - etag_len - ctype_len - tail (a
// v3 entry's metadata tail sits at the very end). A stale or malformed entry
// is a miss (None), never a length.
fn cached_entry_len(state: &AppState, url: &str) -> Option<u64> {
    let path = cache_entry(state, url);
    let mut f = std::fs::File::open(&path).ok()?;
    if !cache_fresh(state, &path) {
        return None;
    }
    // 10-byte fixed prefix: status(2) + etag_len(4) + ctype_len(4)
    let mut hdr = [0u8; 10];
    std::io::Read::read_exact(&mut f, &mut hdr).ok()?;
    let etag_len = u32::from_be_bytes([hdr[2], hdr[3], hdr[4], hdr[5]]) as u64;
    if etag_len == 0 || etag_len > 128 {
        return None;
    }
    let ctype_len = u32::from_be_bytes([hdr[6], hdr[7], hdr[8], hdr[9]]) as u64;
    let file_len = f.metadata().ok()?.len();
    let prefix = 10 + etag_len + ctype_len;
    if file_len < prefix {
        return None;
    }
    // same tail discrimination as cache_get_range_window
    let mut tail_len_buf = [0u8; 4];
    if file_len >= prefix + 4 {
        use std::io::{Seek, SeekFrom};
        f.seek(SeekFrom::End(-4)).ok()?;
        std::io::Read::read_exact(&mut f, &mut tail_len_buf).ok()?;
    }
    let ml = u32::from_be_bytes(tail_len_buf) as u64;
    let tail = if ml >= META_MAGIC.len() as u64
        && ml <= META_LEN_MAX as u64
        && file_len >= prefix + 4 + ml
    {
        4 + ml
    } else {
        0
    };
    Some(file_len - prefix - tail)
}

async fn blob_window(
    State(state): State<AppState>,
    Path((hash, offset, size)): Path<(String, u64, u64)>,
    RawQuery(query): RawQuery,
    headers: HeaderMap,
) -> Response {
    // the hash alone is not reversible, so the client always sends
    // ?u=<percent-encoded upstream url>; the proxy re-derives the disk-cache
    // key from it and refuses a mismatch (keeps cache keys exact without a
    // hash -> url registry). Stays useful for CDN deployments where a
    // pre-mirroring layer knows the hashes. The browser-facing ?v= token is
    // stripped FIRST: cache keys and upstream urls never see it.
    let url = blob_query_url(query.as_deref()).map(percent_decode);
    let Some(url) = url.map(|u| strip_version_param(&u)) else {
        return (StatusCode::BAD_REQUEST, "missing u=<upstream url>").into_response();
    };
    if sha256_hex(url.as_bytes()) != hash {
        return (
            StatusCode::BAD_REQUEST,
            format!("u does not hash to {hash}"),
        )
            .into_response();
    }
    blob_window_serve(state, headers, url, offset, size).await
}

// no-hash form: /blob/<offset>/<size>?u=<url>, for clients that only know
// the proxy-relative resource url a rewritten manifest handed them and can
// therefore not precompute the hash. `u` is either an absolute upstream url
// (cache key re-derived from it, nothing to verify) or a proxy-relative path
// resolved exactly like the matching route above. Unknown prefixes 404.
async fn blob_window_open(
    State(state): State<AppState>,
    Path((offset, size)): Path<(u64, u64)>,
    RawQuery(query): RawQuery,
    headers: HeaderMap,
) -> Response {
    let url = blob_query_url(query.as_deref()).map(percent_decode);
    let Some(url) = url.map(|u| strip_version_param(&u)) else {
        return (StatusCode::BAD_REQUEST, "missing u=<resource url>").into_response();
    };
    let Some(url) = resolve_resource_url(&state, &url) else {
        return (
            StatusCode::NOT_FOUND,
            format!("unknown resource reference: {url}"),
        )
            .into_response();
    };
    blob_window_serve(state, headers, url, offset, size).await
}

// the shared u=<...> query parameter, still percent-encoded (RawQuery is raw)
fn blob_query_url(query: Option<&str>) -> Option<&str> {
    query.and_then(|q| q.split('&').find_map(|kv| kv.strip_prefix("u=")))
}

// map a client-supplied resource reference to the upstream url whose sha256
// is the shared disk-cache key. Absolute urls pass through untouched; a
// proxy-relative path maps to the same upstream base its route uses, with
// the tail percent-decoded once (what axum hands the /patch/* routes; the
// /stream lane additionally insists on an absolute url, like stream_proxy)
fn resolve_resource_url(state: &AppState, reference: &str) -> Option<String> {
    // the browser-facing ?v= version token never reaches an upstream url or a
    // disk-cache key: strip it (and any other v param) BEFORE resolving -
    // see strip_version_param (the linchpin of the cache-busting scheme)
    let reference = strip_version_param(reference);
    if reference.starts_with("http://") || reference.starts_with("https://") {
        return Some(reference);
    }
    let rest = reference.strip_prefix('/')?;
    let (tail, base) = if let Some(t) = rest.strip_prefix("patch/files/") {
        (t, &state.upstream_files)
    } else if let Some(t) = rest.strip_prefix("patch/manifest/") {
        (t, &state.upstream_manifest)
    } else if let Some(t) = rest.strip_prefix("patch/eamythic/") {
        (t, &state.upstream_aops)
    } else if let Some(t) = rest.strip_prefix("data/") {
        // local data mounts: the reference IS the resource (proxy-relative);
        // decoded once like the /stream lane, resolved against the mounts
        return Some(format!("/data/{}", percent_decode(t)));
    } else {
        let t = rest.strip_prefix("stream/")?;
        let url = percent_decode(t);
        return (url.starts_with("http://") || url.starts_with("https://")).then_some(url);
    };
    Some(format!("{base}/{}", percent_decode(tail)))
}

// the window answer shared by both forms: cache-backed or freshly fetched,
// identical bytes and headers either way
async fn blob_window_serve(
    state: AppState,
    headers: HeaderMap,
    url: String,
    offset: u64,
    size: u64,
) -> Response {
    // /data references bypass the whole cache machinery: the source is the
    // local mount, windows pread straight off the file
    if url.starts_with("/data/") {
        return data_blob_window(&state, &headers, &url, offset, size).await;
    }
    // absolute upstream urls (the ?u= open form) face the same SSRF gate as
    // /stream; proxy-relative references resolve through the fixed routes
    if url.starts_with("http://") || url.starts_with("https://") {
        if let Err((status, why)) = admin::upstream_url_allowed(&state, &url).await {
            return (
                StatusCode::from_u16(status).unwrap_or(StatusCode::FORBIDDEN),
                why,
            )
                .into_response();
        }
    }

    // usize windows (saturating on 32-bit targets)
    let start = offset.min(usize::MAX as u64) as usize;
    let end_in = offset
        .saturating_add(size)
        .saturating_sub(1)
        .min(usize::MAX as u64) as usize;

    // v2 disk cache hit: windowed read off disk, the stored resource etag
    // reused as the validator family for the slice
    if state.use_cache {
        if let Some((ctype, etag, total, body)) =
            cache_get_range_window(&state, &url, start, end_in)
        {
            return blob_window_response(
                &headers,
                &sanitize_ctype(&ctype),
                &etag,
                offset,
                total,
                body,
            );
        }
        // no window: a fresh, well-formed entry whose length sits at or
        // before `offset` is a real 416; anything else is a cache miss
        if let Some(total) = cached_entry_len(&state, &url) {
            if offset >= total {
                return blob_unsatisfiable(total);
            }
        }
    }

    // cache miss: one whole upstream GET (no Range - the proxy is the
    // middleman doing the ranging), cached like a files-route 200, then the
    // window served from those exact bytes
    let tail = url
        .strip_prefix(&format!("{}/", state.upstream_files))
        .unwrap_or(&url);
    let resp = match state.http.get(&url).send().await {
        Ok(resp) => resp,
        Err(e) => {
            eprintln!("xuoproxy: blob upstream {url} failed: {e:?}");
            return (
                StatusCode::NOT_FOUND,
                format!("blob source unavailable: {e}"),
            )
                .into_response();
        }
    };
    if !resp.status().is_success() {
        return (
            StatusCode::NOT_FOUND,
            format!("blob upstream status {}", resp.status().as_u16()),
        )
            .into_response();
    }
    let raw = match resp.bytes().await {
        Ok(b) => b.to_vec(),
        Err(e) => {
            return (
                StatusCode::NOT_FOUND,
                format!("blob upstream read failed: {e}"),
            )
                .into_response()
        }
    };
    // the entry is shared with /patch/files: same normalization, so a window
    // never disagrees with what the files route serves for the same hash
    let body = normalize_file_body(tail, raw);
    if state.use_cache {
        cache_put(&state, &url, 200, "application/octet-stream", &body);
    }
    if offset >= body.len() as u64 {
        return blob_unsatisfiable(body.len() as u64);
    }
    let served = if size == 0 {
        Vec::new()
    } else {
        body[start..=end_in.min(body.len() - 1)].to_vec()
    };
    blob_window_response(
        &headers,
        "application/octet-stream",
        &strong_etag(&body),
        offset,
        body.len() as u64,
        served,
    )
}

// 200 with exactly the window bytes: immutable + strong slice etag
// "<stored-resource-etag>-<offset>-<served>" + debug headers. Conditional
// GETs 304 on the slice etag like anywhere else. X-Blob-Total hands the
// client the resource length (its stand-in for a 206's Content-Range total).
fn blob_window_response(
    req_headers: &HeaderMap,
    ctype: &str,
    resource_etag: &str,
    offset: u64,
    total: u64,
    body: Vec<u8>,
) -> Response {
    let served = body.len();
    let etag = format!("\"{}-{offset}-{served}\"", resource_etag.trim_matches('"'));
    if etag_matches(req_headers, &etag) {
        return not_modified_response(&etag, CACHE_IMMUTABLE);
    }
    let mut res = (
        StatusCode::OK,
        [(header::CONTENT_TYPE, ctype.to_string())],
        body,
    )
        .into_response();
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
    hs.insert(
        header::HeaderName::from_static("x-blob-offset"),
        offset.to_string().parse().expect("offset"),
    );
    hs.insert(
        header::HeaderName::from_static("x-blob-size"),
        served.to_string().parse().expect("size"),
    );
    hs.insert(
        header::HeaderName::from_static("x-blob-total"),
        total.to_string().parse().expect("total"),
    );
    res
}

// offset at or past the resource end: a real 416 with the only meaningful
// Content-Range (the client falls back to real ranged requests)
fn blob_unsatisfiable(total: u64) -> Response {
    let mut res = StatusCode::RANGE_NOT_SATISFIABLE.into_response();
    res.headers_mut().insert(
        header::CONTENT_RANGE,
        format!("bytes */{total}").parse().expect("content-range"),
    );
    res
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

// retained for callers that need a cache-distinct key per normalized manifest;
// the streaming path carries the distinction in CacheMeta
#[allow(dead_code)]
fn normalized_manifest_url(manifest_url: &str) -> String {
    format!("{manifest_url}#normalized")
}

// inflate a ct=1 manifest transfer when its stored copy is still compressed
#[allow(dead_code)]
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

fn rewrite_links(state: &AppState, body: &[u8], headers: &HeaderMap) -> Vec<u8> {
    rewrite_links_tokened(state, body, headers, None).0
}

// rewrites absolute upstream urls to same-origin proxy prefixes AND appends a
// browser-facing version token (`?v=<token>`) to every rewritten RESOURCE url
// (a non-empty tail after its route prefix; bare repo roots ending in '/' are
// join anchors - the client concatenates tails onto them, and their contents
// are content-addressed hash names that never change per url - so they stay
// token-free). Token source: the enclosing manifest entry's attrs when the
// url sits inside a MythicMFT tag (its `t` timestamp, else the ul/cl size
// pair), else the manifest-level validator (upstream etag when the caller
// supplies one, else the body hash). Same resource + same token across
// publishes keeps browser caches valid; a changed resource gets a new token
// and browsers re-fetch. Returns the rewritten body plus the upstream urls
// that were mapped (the manifest's reference set, used for targeted content
// invalidation when the manifest changes).
fn rewrite_links_tokened(
    state: &AppState,
    body: &[u8],
    // request headers are threaded through for the caller's API shape; the
    // rewrite itself is header-independent
    _headers: &HeaderMap,
    fallback_token: Option<&str>,
) -> (Vec<u8>, Vec<String>) {
    // only rewrite text bodies (product manifests, pkg.mft xml, patchlists);
    // binary payloads (zlib chunks, uop data) pass through byte-identical
    let text = match std::str::from_utf8(body) {
        Ok(text) => text,
        Err(_) => return (body.to_vec(), Vec::new()),
    };
    if !text.trim_start().starts_with('<') {
        return (body.to_vec(), Vec::new());
    }
    let manifest_token = fallback_token
        .map(str::to_string)
        .unwrap_or_else(|| short_token(body));

    let re = URL_RE.get_or_init(|| regex::Regex::new(r#"https?://[^\s"'<>\\]+"#).unwrap());
    let mut references: Vec<String> = Vec::new();
    let replaced = re.replace_all(text, |caps: &regex::Captures| {
        let upstream = caps[0].trim_end_matches('/');
        // map each known upstream host to its local proxy prefix; the paired
        // cache key is the url THIS proxy's route fetches upstream with (the
        // --*-upstream bases, not the official host in the manifest text) -
        // that is the disk-cache key targeted content invalidation must drop
        let mapped = if upstream
            .starts_with("http://patch.uo.broadsword.com/uopatch-sa/legacyrelease/uo/manifest")
        {
            let rest = upstream
                .trim_start_matches(
                    "http://patch.uo.broadsword.com/uopatch-sa/legacyrelease/uo/manifest",
                )
                .trim_start_matches('/')
                .to_string();
            Some((
                format!("/patch/manifest/{rest}"),
                format!("{}/{}", state.upstream_manifest, rest),
            ))
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
                .and_then(|rest| rest.split_once('/').map(|x| x.1))
                .unwrap_or("")
                .trim_start_matches("legacyrelease/uo/files")
                .trim_start_matches('/')
                .to_string();
            Some((
                format!("/patch/files/{after_version}"),
                format!("{}/{}", state.upstream_files, after_version),
            ))
        } else if upstream.starts_with("http://patch.uo.eamythic.com/uopatch-sa/legacyrelease/uo") {
            let rest = upstream
                .trim_start_matches("http://patch.uo.eamythic.com/uopatch-sa/legacyrelease/uo")
                .to_string();
            Some((
                format!("/patch/eamythic/{rest}"),
                format!("{}/{}", state.upstream_aops, rest.trim_start_matches('/')),
            ))
        } else {
            None
        };
        match mapped {
            Some((prefix, cache_key)) => {
                references.push(cache_key);
                if prefix.ends_with('/') {
                    prefix // repo root: join anchor, stays token-free
                } else {
                    let m = caps.get(0).expect("group 0");
                    let token = resource_version_token(text, m.start(), m.end(), &manifest_token);
                    format!("{prefix}?v={token}")
                }
            }
            None => caps[0].to_string(),
        }
    });
    (replaced.into_owned().into_bytes(), references)
}

// per-resource version token: the enclosing manifest tag's attrs when the url
// sits inside one (a MythicMFT entry's `t` timestamp changes exactly when
// that entry republishes; ul/cl is the fallback pair), else the
// manifest-level validator
// boundary-aware attribute read: `t="` must not match inside `ct="0"` /
// `cl="..."` (the naive needle search normalize::extract_attr uses is fine
// for distinctive names like n=/ul=/cl=, but single-letter attrs collide)
fn attr_exact(tag: &str, name: &str) -> Option<String> {
    let mut from = 0usize;
    while let Some(at) = tag[from..].find(&format!("{name}=\"")) {
        let at = from + at;
        let boundary = at == 0 || !tag.as_bytes()[at - 1].is_ascii_alphanumeric();
        if boundary {
            let rest = &tag[at + name.len() + 2..];
            let end = rest.find('"')?;
            return Some(rest[..end].to_string());
        }
        from = at + name.len() + 2;
    }
    None
}

fn resource_version_token(text: &str, start: usize, end: usize, manifest_token: &str) -> String {
    let tag_start = text[..start].rfind('<').unwrap_or(0);
    let tag_end = text[end..].find('>').map(|i| end + i).unwrap_or(text.len());
    let tag = &text[tag_start..tag_end];
    if let Some(t) = attr_exact(tag, "t") {
        if !t.is_empty() && t != "0" {
            return format!("t{t}");
        }
    }
    let ul = attr_exact(tag, "ul").unwrap_or_default();
    if !ul.is_empty() {
        let cl = attr_exact(tag, "cl").unwrap_or_default();
        return format!("s{}", short_token(format!("{ul}:{cl}").as_bytes()));
    }
    manifest_token.to_string()
}

// ---------------------------------------------------------------------------
// phase 0 freshness: aops flavor sniffing + manifest reference sets
//
// The web client's aops flow (index.html --streaming-product=/stream/<root>/,
// Main.cpp vfs_aops_product_url) puts EVERY aops body on the generic proxy
// lanes: the patchlist at <root>patchlist, the patchcfg at <root>patchcfg
// (its repo keys rewritten to /stream urls), the data files at
// <patch_directory><encoded relpath><custom_extension>. None of those urls
// is content-addressed - upstream replaces the bytes in place - so the lanes
// must revalidate these entries upstream (conditional GET) and, when a body
// changed, invalidate the content entries the OLD body referenced.
// ---------------------------------------------------------------------------

// port of vfs_aops_sniff_patchlist: UTF-16LE BOM plus at least one pipe in
// the first line, or plain text whose first line carries pipes
fn aops_sniff_patchlist(data: &[u8]) -> bool {
    if data.len() < 8 {
        return false;
    }
    if data[0] == 0xFF && data[1] == 0xFE {
        let mut i = 2;
        while i + 1 < data.len() {
            if data[i] == b'\n' && data[i + 1] == 0 {
                break;
            }
            if data[i] == b'|' && data[i + 1] == 0 {
                return true;
            }
            i += 2;
        }
        return false;
    }
    let mut pipe = false;
    for (i, b) in data.iter().take(512).enumerate() {
        if *b == b'\n' || *b == b'\r' {
            return pipe && i > 8;
        }
        if *b == b'|' {
            pipe = true;
        }
    }
    pipe
}

// port of vfs_aops_sniff_patchcfg: the ini carries the
// [patch_system_configuration] section marker
fn aops_sniff_patchcfg(data: &[u8]) -> bool {
    const MARK: &[u8] = b"[patch_system_configuration]";
    if data.len() < 16 {
        return false;
    }
    data.windows(MARK.len()).any(|w| w == MARK)
}

// BOM-detected UTF-16 (LE/BE) to text; plain-ASCII patchlists decode as utf8
fn aops_decode_text(data: &[u8]) -> Option<String> {
    if data.len() < 2 {
        return None;
    }
    let (little_endian, body) = if data.starts_with(&[0xFF, 0xFE]) {
        (true, &data[2..])
    } else if data.starts_with(&[0xFE, 0xFF]) {
        (false, &data[2..])
    } else {
        return std::str::from_utf8(data).ok().map(str::to_string);
    };
    let mut out = String::with_capacity(body.len() / 2);
    for unit in body.as_chunks::<2>().0 {
        let u = if little_endian {
            u16::from_le_bytes([unit[0], unit[1]])
        } else {
            u16::from_be_bytes([unit[0], unit[1]])
        };
        out.push(char::from_u32(u32::from(u)).unwrap_or('\u{FFFD}'));
    }
    Some(out)
}

// port of vfs_aops_url_encode (the client's own per-part encoding, so the
// reference set matches the urls the client actually builds)
fn aops_url_encode(path: &str) -> String {
    let mut out = String::with_capacity(path.len() + 8);
    for b in path.bytes() {
        if b.is_ascii_alphanumeric() || matches!(b, b'-' | b'_' | b'.' | b'~' | b'/') {
            out.push(b as char);
        } else {
            out.push_str(&format!("%{b:02X}"));
        }
    }
    out
}

// mirrors vfs_aops_parse_patchcfg's value handling on the ORIGINAL (never
// rewritten) ini text: repo keys resolved against the patchcfg's upstream
// root, the [project] custom_extension, and the resolved urls as the
// reference set
fn aops_patchcfg_values(text: &str, root: &str) -> (Option<String>, String, Vec<String>) {
    let mut patch_directory = None;
    let mut extension = String::from("aops"); // the client's default
    let mut references: Vec<String> = Vec::new();
    let resolve = |value: &str| -> String {
        if value.starts_with("http://") || value.starts_with("https://") {
            value.to_string()
        } else if let Some(stripped) = value.strip_prefix('/') {
            format!("{root}{stripped}")
        } else {
            format!("{root}{value}")
        }
    };
    let mut section = String::new();
    for line in text.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with(';') || line.starts_with('#') {
            continue;
        }
        if let Some(rest) = line.strip_prefix('[') {
            section = rest.split(']').next().unwrap_or("").trim().to_string();
            continue;
        }
        let Some((key, value)) = line.split_once('=') else {
            continue;
        };
        let (key, value) = (key.trim(), value.trim());
        if value.is_empty() {
            continue;
        }
        match (section.as_str(), key) {
            ("project", "custom_extension") => extension = value.to_string(),
            ("patch_system_configuration", "online_patch_directory")
            | ("patch_system_configuration", "patch_directory") => {
                let mut url = resolve(value);
                if !url.ends_with('/') {
                    url.push('/'); // the client's parse enforces the slash
                }
                references.push(url.clone());
                patch_directory = Some(url);
            }
            ("patch_system_configuration", "online_patchlist_file")
            | ("patch_system_configuration", "patchlist_url") => {
                references.push(resolve(value));
            }
            ("project", "patch_salt_url") => references.push(resolve(value)),
            _ => {}
        }
    }
    (patch_directory, extension, references)
}

// the content entries a patchlist references: every listed relpath resolved
// against the patchcfg's data root with its custom_extension (exactly the
// urls the client's plan build constructs), plus the patchcfg sibling. The
// data root comes from the cached patchcfg entry's metadata; when the
// patchcfg is not cached yet (first boot fetches the patchlist first) one
// small upstream GET fills the gap so first-boot invalidation stays exact.
async fn aops_data_root(
    state: &AppState,
    patchlist_url: &str,
) -> (Option<String>, String, Option<String>) {
    let root = match patchlist_url.rfind('/') {
        Some(at) => &patchlist_url[..=at],
        None => return (None, "aops".to_string(), None),
    };
    let patchcfg_url = format!("{root}patchcfg");
    if let Some(meta) = cache_meta(state, &patchcfg_url) {
        return (
            meta.patch_directory,
            meta.custom_extension.unwrap_or_else(|| "aops".to_string()),
            Some(patchcfg_url),
        );
    }
    match state.http.get(&patchcfg_url).send().await {
        Ok(resp) if resp.status().is_success() => match resp.bytes().await {
            Ok(raw) => {
                let text = String::from_utf8_lossy(&raw);
                let (pd, ce, _) = aops_patchcfg_values(&text, root);
                (pd, ce, Some(patchcfg_url))
            }
            Err(_) => (None, "aops".to_string(), Some(patchcfg_url)),
        },
        _ => (None, "aops".to_string(), Some(patchcfg_url)),
    }
}

// the upstream content urls (disk-cache keys) a patchlist body referenced:
// <data_root><encoded relpath><extension> per ten-field line + the patchcfg
async fn aops_patchlist_references(state: &AppState, url: &str, data: &[u8]) -> Vec<String> {
    let mut references = Vec::new();
    let Some(text) = aops_decode_text(data) else {
        return references;
    };
    let (data_root, extension, patchcfg_url) = aops_data_root(state, url).await;
    let root = match url.rfind('/') {
        Some(at) => &url[..=at],
        None => url,
    };
    if let Some(patchcfg_url) = patchcfg_url {
        references.push(patchcfg_url);
    }
    let data_root = data_root.unwrap_or_else(|| root.to_string());
    for line in text.lines() {
        let line = line.trim_end_matches('\r');
        if line.is_empty() {
            continue;
        }
        // ten pipe-separated fields; field 0 is the backslash relpath
        let fields: Vec<&str> = line.splitn(10, '|').collect();
        if fields.len() != 10 {
            continue;
        }
        let rel = fields[0].replace('\\', "/").trim_matches('/').to_string();
        if rel.is_empty() || rel.contains("..") {
            continue;
        }
        references.push(format!(
            "{data_root}{}{}",
            aops_url_encode(&rel),
            aops_url_encode(&extension)
        ));
    }
    references
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
// cached body serves slices the same way the /patch/files lane does.
// `cache_control` follows the body flavor: bodies the lane sniffed as
// manifests/patchlists/patchcfgs revalidate (no-cache + ETag, they can
// change upstream), content bodies are immutable + ETag; error bodies pass
// through without cache state
fn stream_respond(
    status: u16,
    ctype: &str,
    body: Vec<u8>,
    headers: &HeaderMap,
    cache_control: &str,
) -> Response {
    if status != 200 {
        return respond_plain(status, ctype, body);
    }
    if headers.contains_key(header::RANGE) {
        let etag = strong_etag(&body);
        if let Some(resp) = range_from_cached(ctype, &body, headers, &etag) {
            return resp;
        }
    }
    let status = StatusCode::from_u16(status).unwrap_or(StatusCode::INTERNAL_SERVER_ERROR);
    let etag = strong_etag(&body);
    cacheable_response(status, ctype, body, headers, &etag, cache_control)
}

// classify + process a fetched stream body: aops flavor sniffing (patchlist /
// patchcfg -> manifest entry: revalidated upstream, reference set captured),
// ini rewrite for patchcfg bodies, passthrough for everything else. Returns
// the body to cache/serve and the metadata to persist with it.
async fn stream_process_body(
    state: &AppState,
    url: &str,
    raw: Vec<u8>,
    upstream_etag: Option<String>,
    upstream_lm: Option<String>,
) -> (Vec<u8>, CacheMeta) {
    let validators = (upstream_etag, upstream_lm);
    let meta = |manifest: bool, pd: Option<String>, ce: Option<String>, references: Vec<String>| {
        CacheMeta {
            upstream_etag: validators.0.clone(),
            upstream_last_modified: validators.1.clone(),
            checked_at: Some(now_secs()),
            manifest,
            patch_directory: pd,
            custom_extension: ce,
            references,
        }
    };
    if aops_sniff_patchcfg(&raw) {
        let root = url_up_to_slash(url);
        if let Ok(text) = std::str::from_utf8(&raw) {
            let (pd, ce, mut references) = aops_patchcfg_values(text, &root);
            references.push(format!("{root}patchlist"));
            let body = stream_rewrite_aops(&raw, url);
            let rewrote = body != raw;
            println!(
                "xuoproxy: stream patchcfg {url} processed ({} repo refs, ini rewrite: {})",
                references.len(),
                rewrote
            );
            return (body, meta(true, pd, Some(ce), references));
        }
        return (raw, meta(true, None, None, Vec::new()));
    }
    if aops_sniff_patchlist(&raw) {
        let references = aops_patchlist_references(state, url, &raw).await;
        println!(
            "xuoproxy: stream patchlist {url} processed ({} content refs)",
            references.len()
        );
        return (raw, meta(true, None, None, references));
    }
    (raw, meta(false, None, None, Vec::new()))
}

fn url_up_to_slash(url: &str) -> String {
    match url.rfind('/') {
        Some(at) => url[..=at].to_string(),
        None => url.to_string(),
    }
}

// the generic lane's manifest classifier: same aops sniffing as the stream
// lane, but the body is cached RAW and rewritten at serve time (the lane's
// own semantics); only the metadata (manifest flag + reference set) is added
async fn generic_manifest_process(
    state: &AppState,
    url: &str,
    raw: Vec<u8>,
    upstream_etag: Option<String>,
    upstream_lm: Option<String>,
) -> (Vec<u8>, CacheMeta) {
    let meta = |manifest: bool, pd: Option<String>, ce: Option<String>, references: Vec<String>| {
        CacheMeta {
            upstream_etag: upstream_etag.clone(),
            upstream_last_modified: upstream_lm.clone(),
            checked_at: Some(now_secs()),
            manifest,
            patch_directory: pd,
            custom_extension: ce,
            references,
        }
    };
    if aops_sniff_patchcfg(&raw) {
        let root = url_up_to_slash(url);
        if let Ok(text) = std::str::from_utf8(&raw) {
            let (pd, ce, mut references) = aops_patchcfg_values(text, &root);
            references.push(format!("{root}patchlist"));
            return (raw, meta(true, pd, Some(ce), references));
        }
        return (raw, meta(true, None, None, Vec::new()));
    }
    if aops_sniff_patchlist(&raw) {
        let references = aops_patchlist_references(state, url, &raw).await;
        return (raw, meta(true, None, None, references));
    }
    if let Ok(text) = std::str::from_utf8(&raw) {
        if text.trim_start().starts_with('<') {
            // XML-ish text on this lane: record which upstream urls it
            // references (the token run happens at serve time)
            let fallback = match &upstream_etag {
                Some(etag) => short_token(etag.as_bytes()),
                None => short_token(&raw),
            };
            let (_, references) =
                rewrite_links_tokened(state, &raw, &HeaderMap::new(), Some(&fallback));
            return (raw, meta(true, None, None, references));
        }
    }
    (raw, meta(false, None, None, Vec::new()))
}

// the freshness lane shared by the /stream and generic routes for cached
// MANIFEST-FLAVORED entries: throttled conditional GET upstream, 304 serves
// the cached body, 200 re-processes and - when the body actually changed -
// invalidates the content entries the old body referenced, upstream errors
// serve stale. The `serve` shaping is the caller's (its range/rewrite
// semantics differ); `process` turns a fresh upstream body into (body, meta).
// 8 params by design: the caller owns the request lane (url/status/ctype/
// body/meta) and the response shaping (process/serve) for it
#[allow(clippy::too_many_arguments)]
async fn manifest_entry_serve(
    state: &AppState,
    url: &str,
    status: u16,
    ctype: &str,
    body: Vec<u8>,
    meta: &CacheMeta,
    process: impl AsyncFnOnce(Vec<u8>, Option<String>, Option<String>) -> (Vec<u8>, CacheMeta),
    // response shaping for the caller's lane; the caller's request headers
    // (validators, range) are captured here
    serve: impl Fn(u16, &str, Vec<u8>) -> Response,
) -> Response {
    if !manifest_revalidate_due(state, meta) {
        println!(
            "xuoproxy: manifest {url} served from cache, upstream check deferred by manifest-ttl {}s",
            state.manifest_ttl
        );
        return serve(status, ctype, body);
    }
    match conditional_get(state, url, meta).await {
        UpstreamCheck::NotModified => {
            println!(
                "xuoproxy: manifest {url} upstream 304, cached copy is current ({} bytes)",
                body.len()
            );
            cache_touch(state, url, status, ctype, &body, meta);
            serve(status, ctype, body)
        }
        UpstreamCheck::Modified(resp) => {
            let (etag, lm) = response_validators(&resp);
            let raw = match resp.bytes().await {
                Ok(b) => b.to_vec(),
                Err(e) => {
                    eprintln!("xuoproxy: manifest {url} upstream read failed ({e}), serving stale");
                    return serve(status, ctype, body);
                }
            };
            let (new_body, new_meta) = process(raw, etag.clone(), lm.clone()).await;
            let changed =
                new_body != body || meta.upstream_etag != etag || meta.upstream_last_modified != lm;
            if changed {
                println!(
                    "xuoproxy: manifest {url} CHANGED upstream ({} -> {} bytes), invalidating old references",
                    body.len(),
                    new_body.len()
                );
                invalidate_references(state, &meta.references);
            } else {
                println!("xuoproxy: manifest {url} upstream 200, rebuilt body identical");
            }
            cache_put_with_meta(state, url, 200, ctype, &new_body, &new_meta);
            serve(200, ctype, new_body)
        }
        UpstreamCheck::Other(status) => {
            eprintln!("xuoproxy: manifest {url} upstream status {status}, serving stale");
            serve(status, ctype, body)
        }
        UpstreamCheck::Failed(e) => {
            eprintln!("xuoproxy: manifest {url} upstream unreachable ({e}), serving stale");
            serve(status, ctype, body)
        }
    }
}

async fn stream_proxy(
    State(state): State<AppState>,
    Path(url): Path<String>,
    headers: HeaderMap,
) -> Response {
    // axum already percent-decoded the wildcard; validate the scheme. A
    // browser-facing ?v= token (possibly traveling inside the encoded url)
    // never reaches the upstream url or the disk-cache key.
    let url = strip_version_param(&percent_decode(&url));
    let url = repair_scheme_slash(&url);
    if !(url.starts_with("http://") || url.starts_with("https://")) {
        return (StatusCode::BAD_REQUEST, "expected an absolute http(s) url").into_response();
    }
    if let Err((status, why)) = admin::upstream_url_allowed(&state, &url).await {
        return (
            StatusCode::from_u16(status).unwrap_or(StatusCode::FORBIDDEN),
            why,
        )
            .into_response();
    }

    if state.use_cache {
        if let Some((status, ctype, _etag, body)) = cache_get(&state, &url) {
            let meta = cache_meta(&state, &url).unwrap_or_default();
            if meta.manifest && status == 200 {
                return manifest_entry_serve(
                    &state,
                    &url,
                    status,
                    &sanitize_ctype(&ctype),
                    body,
                    &meta,
                    async |raw, etag, lm| stream_process_body(&state, &url, raw, etag, lm).await,
                    |status, ctype, body| {
                        stream_respond(status, ctype, body, &headers, CACHE_REVALIDATE)
                    },
                )
                .await;
            }
            return stream_respond(status, &ctype, body, &headers, CACHE_IMMUTABLE);
        }
    }

    let resp = match state.http.get(&url).send().await {
        Ok(resp) => resp,
        Err(e) => {
            return (
                StatusCode::BAD_GATEWAY,
                upstream_error_body(&state, "fetch", &e),
            )
                .into_response()
        }
    };

    let status = resp.status().as_u16();
    let (upstream_etag, upstream_lm) = response_validators(&resp);
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
                upstream_error_body(&state, "read", &e),
            )
                .into_response()
        }
    };

    if status == 200 {
        // an aops patchcfg is rewritten to same-origin /stream urls before it
        // is cached or served (200 only: error bodies pass through
        // untouched); patchlists/patchcfgs are marked manifest and carry
        // their reference set for targeted invalidation
        let (body, meta) =
            stream_process_body(&state, &url, body, upstream_etag, upstream_lm).await;
        if state.use_cache {
            cache_put_with_meta(&state, &url, status, &ctype, &body, &meta);
        }
        return stream_respond(
            status,
            &ctype,
            body,
            &headers,
            if meta.manifest {
                CACHE_REVALIDATE
            } else {
                CACHE_IMMUTABLE
            },
        );
    }

    stream_respond(status, &ctype, body, &headers, CACHE_IMMUTABLE)
}

fn respond_plain(status: u16, ctype: &str, body: Vec<u8>) -> Response {
    let status = StatusCode::from_u16(status).unwrap_or(StatusCode::INTERNAL_SERVER_ERROR);
    (status, [(header::CONTENT_TYPE, ctype.to_string())], body).into_response()
}

// Reverse proxies in front of xuoproxy (Ferron's url sanitizer, nginx
// merge_slashes, ...) normalize the request path and collapse the `//` of the
// embedded upstream scheme (`/stream/https://host/...` arrives as
// `/stream/https:/host/...`). Re-insert the missing slash before validation,
// so the proxy works without touching the front-end config.
fn repair_scheme_slash(url: &str) -> String {
    for scheme in ["https:/", "http:/"] {
        if let Some(rest) = url.strip_prefix(scheme) {
            if !rest.starts_with('/') {
                return format!("{}/{}", scheme, rest);
            }
        }
    }
    url.to_string()
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

async fn ws_shard(
    State(state): State<AppState>,
    Path((host, port)): Path<(String, u16)>,
    upgrade: WebSocketUpgrade,
) -> Response {
    // TCP-facing SSRF gate: only LoginServer addresses from shards.cfg
    // (plus explicit --ws-allow entries) may be bridged on a locked server
    if !state.admin.ws_allowed(&host, port) {
        state.admin.stats.ws_fail();
        return (
            StatusCode::FORBIDDEN,
            format!("shard {host}:{port} is not on the ws allowlist"),
        )
            .into_response();
    }
    upgrade.on_upgrade(move |socket| async move { bridge(&state, socket, host, port).await })
}

async fn bridge(state: &AppState, socket: WebSocket, host: String, port: u16) {
    let tcp = match tokio::net::TcpStream::connect((host.as_str(), port)).await {
        Ok(tcp) => tcp,
        Err(e) => {
            eprintln!("xuoproxy: ws bridge to {host}:{port} failed: {e}");
            state.admin.stats.ws_fail();
            return;
        }
    };
    state.admin.stats.ws_open();
    println!("xuoproxy: ws bridge open -> {host}:{port}");
    let started = std::time::Instant::now();
    let up = std::sync::Arc::new(std::sync::atomic::AtomicU64::new(0));
    let down = std::sync::Arc::new(std::sync::atomic::AtomicU64::new(0));
    let up_to_shard = up.clone();
    let down_to_client = down.clone();

    let (mut ws_tx, mut ws_rx) = socket.split();
    let (mut tcp_rx, mut tcp_tx) = tokio::io::split(tcp);

    // client -> shard: binary websocket frames are raw protocol bytes
    let to_shard = async move {
        while let Some(Ok(msg)) = ws_rx.next().await {
            match msg {
                axum::extract::ws::Message::Binary(data) => {
                    up_to_shard.fetch_add(data.len() as u64, Ordering::Relaxed);
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
                    down_to_client.fetch_add(n as u64, Ordering::Relaxed);
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
    state.admin.stats.ws_close(
        started.elapsed(),
        up.load(Ordering::Relaxed),
        down.load(Ordering::Relaxed),
    );
    println!("xuoproxy: ws bridge closed -> {host}:{port}");
}

// ---------------------------------------------------------------------------
// /data/<name>/<path>: local data mounts (shard-owned UO installs)
// ---------------------------------------------------------------------------

// files up to this size are read whole, which doubles as the aops flavor
// sniff (patchlist/patchcfg bodies need in-memory rewriting); anything
// bigger is a data payload and streams straight from disk
const DATA_INLINE_MAX: u64 = 1024 * 1024;
// in-memory cap for one ranged read; bigger ranges stream from the seeked
// file instead of materializing
const DATA_WINDOW_MAX: u64 = 8 * 1024 * 1024;
// synthesis walk limits: a mount is one shard's install, not a filesystem
const DATA_WALK_MAX_FILES: usize = 65536;
const DATA_WALK_MAX_DEPTH: usize = 8;

fn data_dir_of(state: &AppState, name: &str) -> Option<PathBuf> {
    if name.is_empty()
        || !name
            .bytes()
            .all(|b| b.is_ascii_alphanumeric() || b == b'_' || b == b'-')
    {
        return None;
    }
    state
        .data_dirs
        .lock()
        .expect("data mounts")
        .get(name)
        .cloned()
}

// the client appends cache-busting query tokens to resource references
// the client appends cache-busting query tokens to resource references
// ("?xuo=<offset>", rewritten-manifest "?v=<token>"); a filesystem path ends
// at the first such token
fn data_strip_client_params(tail: &str) -> &str {
    let mut cut = tail.len();
    for pat in ["?xuo=", "&xuo=", "?v=", "&v="] {
        if let Some(at) = tail.find(pat) {
            cut = cut.min(at);
        }
    }
    &tail[..cut]
}

// resolve a request tail against a mount root: component-wise join under a
// strict grammar (no traversal, no backslashes - the wire format uses them
// as separators, never as name bytes) with a case-insensitive fallback for
// the LAST component: the client requests lowercased basenames (the plan
// lowercases on install) while legacy installs ship "ART.MUL"-style names
async fn data_resolve(base: &std::path::Path, tail: &str) -> Option<PathBuf> {
    let comps: Vec<&str> = tail
        .split('/')
        .filter(|c| !c.is_empty() && *c != ".")
        .collect();
    let last = comps.len().checked_sub(1)?;
    let mut cur = base.to_path_buf();
    for (i, comp) in comps.iter().enumerate() {
        if *comp == ".." || comp.contains('\\') || comp.contains('\0') {
            return None;
        }
        let candidate = cur.join(comp);
        if tokio::fs::metadata(&candidate).await.is_ok() {
            cur = candidate;
            continue;
        }
        if i == last {
            let mut matched: Option<PathBuf> = None;
            if let Ok(mut rd) = tokio::fs::read_dir(&cur).await {
                let want = comp.to_ascii_lowercase();
                while let Ok(Some(entry)) = rd.next_entry().await {
                    if entry.file_name().to_string_lossy().to_ascii_lowercase() == want {
                        matched = Some(cur.join(entry.file_name()));
                        break;
                    }
                }
            }
            if let Some(p) = matched {
                cur = p;
                continue;
            }
        }
        return None;
    }
    Some(cur)
}

// cheap validator for local bodies: content is served from the owner's disk,
// so the etag rides on (size, mtime) - no whole-file read per request
fn data_etag(len: u64, mtime: Option<(u64, u32)>) -> String {
    let (secs, nanos) = mtime.unwrap_or((0, 0));
    format!(
        "\"{:016x}\"",
        fnv1a64(format!("data:{len}:{secs}:{nanos}").as_bytes())
    )
}

async fn data_mtime(meta: &std::fs::Metadata) -> Option<(u64, u32)> {
    meta.modified()
        .ok()?
        .duration_since(std::time::UNIX_EPOCH)
        .ok()
        .map(|d| (d.as_secs(), d.subsec_nanos()))
}

// single-range parser: "bytes=start-end" (inclusive), "bytes=start-" and
// "bytes=-suffix"; returns the inclusive (start, end) clamped to the file.
// None when malformed or unsatisfiable (the caller then serves the whole
// body or a 416 - the client only ever sends one explicit range)
fn data_parse_range(headers: &HeaderMap, len: u64) -> Option<(u64, u64)> {
    let spec = headers.get(header::RANGE)?.to_str().ok()?;
    let spec = spec.trim().strip_prefix("bytes=")?;
    let (start_s, end_s) = spec.trim().split_once('-')?;
    if start_s.is_empty() {
        let suffix: u64 = end_s.trim().parse().ok()?;
        if suffix == 0 || len == 0 {
            return None;
        }
        return Some((len.saturating_sub(suffix), len - 1));
    }
    let start: u64 = start_s.trim().parse().ok()?;
    let end = if end_s.is_empty() {
        len.checked_sub(1)?
    } else {
        end_s.trim().parse::<u64>().ok()?.min(len.saturating_sub(1))
    };
    if start > end || start >= len {
        return None;
    }
    Some((start, end))
}

// disk-backed streaming body: 256 KiB chunks via unfold, so whole-file reads
// of multi-GB .uop/.mul payloads never materialize in memory
fn data_stream(file: tokio::fs::File, remaining: u64) -> axum::body::Body {
    axum::body::Body::from_stream(futures_util::stream::unfold(
        (file, remaining),
        |(mut file, mut remaining)| async move {
            if remaining == 0 {
                return None;
            }
            let want = usize::try_from(remaining.min(256 * 1024)).unwrap_or(256 * 1024);
            let mut buf = vec![0u8; want];
            match file.read(&mut buf).await {
                Ok(0) | Err(_) => None,
                Ok(n) => {
                    remaining -= n as u64;
                    buf.truncate(n);
                    Some((
                        Ok::<_, std::io::Error>(axum::body::Bytes::from(buf)),
                        (file, remaining),
                    ))
                }
            }
        },
    ))
}

// the UO data filter, ported from vfs_source_aops.cpp (vfs_aops_filter_accept
// and its tables): what a client install is made of. The generated MythicMFT
// manifest is the only filter the client sees on this lane, so the tables
// must stay in lockstep with the client's own notion of UO data.
const DATA_EXT: [&str; 6] = [".mul", ".idx", ".uop", ".def", ".bin", ".iff"];
const DATA_LANG_STEMS: [&str; 15] = [
    "cliloc.",
    "skilname.",
    "skill16.",
    "skill30.",
    "chat.",
    "gesture.",
    "intloc",
    "intro.",
    "options.",
    "optnuotd.",
    "optuotd.",
    "professn.",
    "tilehelp.",
    "tooltips.",
    "gt_",
];
const DATA_LANGS: [&str; 8] = [
    ".cht", ".chs", ".deu", ".enu", ".esp", ".fra", ".jpn", ".kor",
];
const DATA_NAMES: [&str; 28] = [
    "arttable.cfg",
    "blueprints.tbp",
    "conf_ignore.lst",
    "credits.dat",
    "default.mac",
    "doors.txt",
    "floors.txt",
    "gamegen.txt",
    "helpindx.dat",
    "ignore.lst",
    "keynames.txt",
    "langcode.iff",
    "macros.txt",
    "macros2d.txt",
    "misc.txt",
    "mobtypes.txt",
    "multimap.rle",
    "obscene.lst",
    "prof.txt",
    "roof.txt",
    "stairs.txt",
    "teleprts.txt",
    "tile1024.txt",
    "tile256.txt",
    "uobscene.lst",
    "version.dat",
    "version.txt",
    "walls.txt",
];

fn data_has_suffix(lower: &str, exts: &[&str]) -> bool {
    exts.iter()
        .any(|e| lower.len() > e.len() && lower.ends_with(e))
}

fn data_filter_accept(base_lower: &str) -> bool {
    if base_lower.is_empty() {
        return false;
    }
    if data_has_suffix(base_lower, &DATA_EXT) {
        return true;
    }
    if data_has_suffix(base_lower, &DATA_LANGS) {
        return DATA_LANG_STEMS.iter().any(|s| base_lower.starts_with(s));
    }
    DATA_NAMES.contains(&base_lower)
}

struct DataEntry {
    rel: String, // '/'-separated, original directory case, lowercased basename
    len: u64,
    mtime: Option<(u64, u32)>,
}

async fn data_walk(dir: &std::path::Path, rel: &str, out: &mut Vec<DataEntry>, depth: usize) {
    if depth > DATA_WALK_MAX_DEPTH || out.len() >= DATA_WALK_MAX_FILES {
        return;
    }
    let Ok(mut rd) = tokio::fs::read_dir(dir).await else {
        return;
    };
    while let Ok(Some(entry)) = rd.next_entry().await {
        // symlinks are skipped outright: no loops, no escaping the mount
        let Ok(ft) = entry.file_type().await else {
            continue;
        };
        if ft.is_symlink() {
            continue;
        }
        let fname = entry.file_name().to_string_lossy().to_string();
        let child_rel = if rel.is_empty() {
            fname.clone()
        } else {
            format!("{rel}/{fname}")
        };
        if ft.is_dir() {
            Box::pin(data_walk(&dir.join(&fname), &child_rel, out, depth + 1)).await;
            continue;
        }
        if !data_filter_accept(&fname.to_ascii_lowercase()) {
            continue;
        }
        let Ok(meta) = entry.metadata().await else {
            continue;
        };
        if !meta.is_file() {
            continue;
        }
        if out.len() >= DATA_WALK_MAX_FILES {
            return;
        }
        // the wire relpath's basename is emitted LOWERCASED because that is
        // exactly the url the client will request (vfs_source_aops.cpp
        // lowercases before building the remote path); the file route's
        // case-insensitive last-component fallback bridges the difference to
        // the on-disk name
        let lowered = match child_rel.rfind('/') {
            Some(at) => format!(
                "{}{}",
                &child_rel[..at + 1],
                child_rel[at + 1..].to_ascii_lowercase()
            ),
            None => child_rel.to_ascii_lowercase(),
        };
        out.push(DataEntry {
            rel: lowered,
            len: meta.len(),
            mtime: data_mtime(&meta).await,
        });
    }
}

// lookup3 hashlittle2 (Bob Jenkins), the byte-at-a-time path - the client
// derives every MythicMFT loose-file resource name from it:
// mft_entry_remote_name hashlittle2()s the LOWERCASED file name with the
// entry's ph/sh as seeds (absent attributes parse as 0) and fetches
// "<rpath>/unpacked/<%08x%08x of ph,sh>". Ported verbatim (external/lookup3.h)
// so the names this proxy generates and serves match what the client requests.
fn lookup3_hashlittle2(key: &[u8], pc: u32, pb: u32) -> (u32, u32) {
    #[inline]
    fn rot(x: u32, k: u32) -> u32 {
        x.rotate_left(k)
    }
    macro_rules! mix {
        ($a:expr, $b:expr, $c:expr) => {{
            $a = $a.wrapping_sub($c);
            $a ^= rot($c, 4);
            $c = $c.wrapping_add($b);
            $b = $b.wrapping_sub($a);
            $b ^= rot($a, 6);
            $a = $a.wrapping_add($c);
            $c = $c.wrapping_sub($b);
            $c ^= rot($b, 8);
            $b = $b.wrapping_add($a);
            $a = $a.wrapping_sub($c);
            $a ^= rot($c, 16);
            $c = $c.wrapping_add($b);
            $b = $b.wrapping_sub($a);
            $b ^= rot($a, 19);
            $a = $a.wrapping_add($c);
            $c = $c.wrapping_sub($b);
            $c ^= rot($b, 4);
            $b = $b.wrapping_add($a);
        }};
    }
    macro_rules! hash_final {
        ($a:expr, $b:expr, $c:expr) => {{
            $c ^= $b;
            $c = $c.wrapping_sub(rot($b, 14));
            $a ^= $c;
            $a = $a.wrapping_sub(rot($c, 11));
            $b ^= $a;
            $b = $b.wrapping_sub(rot($a, 25));
            $c ^= $b;
            $c = $c.wrapping_sub(rot($b, 16));
            $a ^= $c;
            $a = $a.wrapping_sub(rot($c, 4));
            $b ^= $a;
            $b = $b.wrapping_sub(rot($a, 14));
            $c ^= $b;
            $c = $c.wrapping_sub(rot($b, 24));
        }};
    }

    let mut len = key.len();
    let a = 0xdeadbeefu32.wrapping_add(len as u32).wrapping_add(pc);
    let mut a = a;
    let mut b = a;
    let mut c = a.wrapping_add(pb);

    let mut k = key;
    while len > 12 {
        a = a.wrapping_add(u32::from_le_bytes([k[0], k[1], k[2], k[3]]));
        b = b.wrapping_add(u32::from_le_bytes([k[4], k[5], k[6], k[7]]));
        c = c.wrapping_add(u32::from_le_bytes([k[8], k[9], k[10], k[11]]));
        mix!(a, b, c);
        len -= 12;
        k = &k[12..];
    }
    // the last block: the switch's fall-through chain adds the remaining
    // bytes little-endian across a/b/c (len == 0 returns without mixing)
    if len > 0 {
        if len >= 4 {
            a = a.wrapping_add(u32::from_le_bytes([k[0], k[1], k[2], k[3]]));
        } else {
            let mut v = 0u32;
            for (i, byte) in k[..len].iter().enumerate() {
                v |= u32::from(*byte) << (8 * i);
            }
            a = a.wrapping_add(v);
        }
        if len > 4 {
            if len >= 8 {
                b = b.wrapping_add(u32::from_le_bytes([k[4], k[5], k[6], k[7]]));
            } else {
                let mut v = 0u32;
                for (i, byte) in k[4..len].iter().enumerate() {
                    v |= u32::from(*byte) << (8 * i);
                }
                b = b.wrapping_add(v);
            }
        }
        if len > 8 {
            if len >= 12 {
                c = c.wrapping_add(u32::from_le_bytes([k[8], k[9], k[10], k[11]]));
            } else {
                let mut v = 0u32;
                for (i, byte) in k[8..len].iter().enumerate() {
                    v |= u32::from(*byte) << (8 * i);
                }
                c = c.wrapping_add(v);
            }
        }
        hash_final!(a, b, c);
    }
    (c, b)
}

// the resource name of a loose file: "%08x%08x" of hashlittle2(lowercase name)
// seeded with the entry's (absent => zero) ph/sh
fn data_blob_name(name_lower: &str) -> String {
    let (ph, sh) = lookup3_hashlittle2(name_lower.as_bytes(), 0, 0);
    format!("{ph:08x}{sh:08x}")
}

// ---------------------------------------------------------------------------
// the generated product: a raw install folder is presented to the client as a
// first-class MythicMFT product, built on the fly and cached
//
// The client's --streaming-product always lands on "<root>patchlist" (Main.cpp
// vfs_aops_product_url) and the product ladder dispatches on CONTENT
// (vfs_manifest_build), so the generated product manifest is served at
// /data/<mount>/patchlist and plans through the exact same MythicMFT path as
// the official CDN: the product references one sub-manifest
// (base/pkg.mft, rpath "base" - the top level cannot carry <files>, the plan
// build skips entries whose remote_path is null and only packages confer one),
// whose loose files fetch from <filerepos>/base/unpacked/<name hash>.
// Every entry is stored (ct=0): the mount serves raw bytes, big .uop files
// refine into block-range assets via ranged reads after the plan walks them.
// ---------------------------------------------------------------------------

struct DataProduct {
    // content fingerprint of the listing: the product serial, so a changed
    // folder bumps the client's plan drift on the next boot
    serial: u64,
    product: Vec<u8>,
    pkg: Vec<u8>,
    // name hash -> file on disk
    blobs: HashMap<String, PathBuf>,
}

static DATA_PRODUCTS: OnceLock<Mutex<HashMap<String, Arc<DataProduct>>>> = OnceLock::new();

fn data_products() -> &'static Mutex<HashMap<String, Arc<DataProduct>>> {
    DATA_PRODUCTS.get_or_init(|| Mutex::new(HashMap::new()))
}

fn xml_escape(text: &str) -> String {
    let mut out = String::with_capacity(text.len());
    for c in text.chars() {
        match c {
            '&' => out.push_str("&amp;"),
            '<' => out.push_str("&lt;"),
            '>' => out.push_str("&gt;"),
            '"' => out.push_str("&quot;"),
            '\'' => out.push_str("&apos;"),
            _ => out.push(c),
        }
    }
    out
}

// the walk is also the cache key: relpath + size + mtime fingerprint
// (FNV-1a 64 continuation over the whole listing, same constants as fnv1a64)
fn data_listing_serial(entries: &[DataEntry]) -> u64 {
    let mut hash = 0xcbf29ce484222325u64;
    let step = |hash: &mut u64, bytes: &[u8]| {
        for byte in bytes {
            *hash ^= u64::from(*byte);
            *hash = hash.wrapping_mul(0x100000001b3);
        }
    };
    for entry in entries {
        step(&mut hash, entry.rel.as_bytes());
        step(&mut hash, &entry.len.to_le_bytes());
        step(&mut hash, &entry.mtime.unwrap_or((0, 0)).0.to_le_bytes());
    }
    hash
}

async fn data_build_product(repo_url: &str, base: PathBuf) -> Option<Arc<DataProduct>> {
    let mut entries: Vec<DataEntry> = Vec::new();
    data_walk(&base, "", &mut entries, 0).await;
    entries.sort_by(|a, b| a.rel.cmp(&b.rel));
    // one entry per lowercased basename (the install name is flat), keeping
    // the entry whose on-disk name is ALREADY lowercase: n names the install
    // file and the blob tail derives from it
    let mut seen: HashSet<String> = HashSet::new();
    let mut chosen: Vec<DataEntry> = Vec::new();
    for entry in entries {
        let basename = entry
            .rel
            .rsplit('/')
            .next()
            .unwrap_or(&entry.rel)
            .to_string();
        if !seen.insert(basename.clone()) {
            if entry.rel == entry.rel.to_lowercase() {
                if let Some(slot) = chosen
                    .iter_mut()
                    .find(|e| e.rel.rsplit('/').next() == Some(basename.as_str()))
                {
                    *slot = entry;
                }
            }
            continue;
        }
        chosen.push(entry);
    }
    if chosen.is_empty() {
        return None;
    }
    let serial = data_listing_serial(&chosen);

    let mut blobs: HashMap<String, PathBuf> = HashMap::with_capacity(chosen.len());
    let mut files_xml = String::with_capacity(chosen.len() * 96);
    let mut max_mtime = 0u64;
    for entry in &chosen {
        let basename = entry.rel.rsplit('/').next().unwrap_or(&entry.rel);
        let blob = data_blob_name(basename);
        // the wire relpath carries the lowercased basename; resolve the REAL
        // on-disk path (the case-insensitive last-component fallback bridges
        // legacy installs like ART.MUL)
        let real = match data_resolve(&base, &entry.rel).await {
            Some(real) => real,
            None => base.join(&entry.rel),
        };
        blobs.insert(blob.clone(), real);
        let (secs, _) = entry.mtime.unwrap_or((0, 0));
        max_mtime = max_mtime.max(secs);
        files_xml.push_str(&format!(
            "            <f n=\"{}\" ul=\"{:x}\" ct=\"0\" cl=\"{:x}\" t=\"{:x}\" rh=\"0\" />\r\n",
            xml_escape(basename),
            entry.len,
            entry.len,
            secs
        ));
    }

    let pkg = format!(
        "<MythicMFT>\r\n    <manifest>\r\n        <files>\r\n{}        </files>\r\n    </manifest>\r\n</MythicMFT>\r\n",
        files_xml
    );
    let pkg_len = pkg.len();
    let product = format!(
        "<MythicMFT>\r\n    <product serial=\"{serial}\" launchfile=\"client.exe\">\r\n        <manifestrepos>\r\n            <repo url=\"{repo_url}\" />\r\n        </manifestrepos>\r\n        <filerepos>\r\n            <repo url=\"{repo_url}\" />\r\n        </filerepos>\r\n        <stages>\r\n            <stage name=\"main\" priority=\"10\" verifyunpacked=\"0\">\r\n                <completion />\r\n                <packages>\r\n                    <package name=\"base\" rpath=\"base\" priority=\"0\">\r\n                        <manifest n=\"pkg.mft\" ul=\"{pkg_len:x}\" ct=\"0\" cl=\"{pkg_len:x}\" t=\"{max_mtime:x}\" rh=\"0\" />\r\n                    </package>\r\n                </packages>\r\n            </stage>\r\n        </stages>\r\n    </product>\r\n</MythicMFT>\r\n"
    );
    Some(Arc::new(DataProduct {
        serial,
        product: product.into_bytes(),
        pkg: pkg.into_bytes(),
        blobs,
    }))
}

// --generate-product: write the generated product out as a plain folder that
// any static web server can host: uo-legacyrelease.prod (product manifest),
// base/pkg.mft (sub-manifest) and base/unpacked/<name hash> (the loose blobs,
// hardlinked when the output lands on the same filesystem, copied otherwise).
// The repo url prefix is baked into both manifests, so it must match the web
// path the folder is served from (--generate-url).
async fn generate_product_folder(
    src: &std::path::Path,
    out: &std::path::Path,
    repo_url: &str,
) -> Result<(u64, usize), String> {
    let product = data_build_product(repo_url, src.to_path_buf())
        .await
        .ok_or_else(|| format!("no files found under {}", src.display()))?;
    let unpacked = out.join("base").join("unpacked");
    tokio::fs::create_dir_all(&unpacked)
        .await
        .map_err(|e| format!("create {}: {e}", unpacked.display()))?;
    tokio::fs::write(out.join("uo-legacyrelease.prod"), &product.product)
        .await
        .map_err(|e| format!("write uo-legacyrelease.prod: {e}"))?;
    tokio::fs::write(out.join("base").join("pkg.mft"), &product.pkg)
        .await
        .map_err(|e| format!("write base/pkg.mft: {e}"))?;
    let mut count = 0usize;
    for (hash, real) in &product.blobs {
        let dest = unpacked.join(hash);
        let _ = tokio::fs::remove_file(&dest).await;
        if tokio::fs::hard_link(real, &dest).await.is_err() {
            // different filesystem (or hardlinks unsupported): fall back to a
            // full copy
            tokio::fs::copy(real, &dest)
                .await
                .map_err(|e| format!("copy {} -> {}: {e}", real.display(), dest.display()))?;
        }
        count += 1;
    }
    Ok((product.serial, count))
}

// the mount's cached product, rebuilt when the listing fingerprint changes
async fn data_product(state: &AppState, name: &str) -> Option<Arc<DataProduct>> {
    let base = data_dir_of(state, name)?;
    let fresh = data_build_product(&format!("/data/{name}/"), base).await?;
    let mut cache = data_products().lock().expect("data products");
    match cache.get(name) {
        Some(cached) if cached.serial == fresh.serial => Some(cached.clone()),
        _ => {
            cache.insert(name.to_string(), fresh.clone());
            Some(fresh)
        }
    }
}

// conditional + ranged response for an in-memory body (small files and the
// synthesized products)
fn data_inline_response(
    headers: &HeaderMap,
    body: Vec<u8>,
    ctype: &str,
    cache_control: &str,
) -> Response {
    let etag = strong_etag(&body);
    if etag_matches(headers, &etag) {
        return not_modified_response(&etag, cache_control);
    }
    let len = body.len();
    if let Some((start, end)) = data_parse_range(headers, len as u64) {
        let (start, end) = (start as usize, end as usize);
        let mut res = (
            StatusCode::PARTIAL_CONTENT,
            [(header::CONTENT_TYPE, ctype.to_string())],
            body[start..=end].to_vec(),
        )
            .into_response();
        let hs = res.headers_mut();
        hs.insert(
            header::CONTENT_RANGE,
            format!("bytes {start}-{end}/{len}")
                .parse()
                .expect("content-range"),
        );
        hs.insert(header::ETAG, etag.parse().expect("etag"));
        hs.insert(
            header::CACHE_CONTROL,
            cache_control.parse().expect("cache-control"),
        );
        hs.insert(
            header::ACCEPT_RANGES,
            header::HeaderValue::from_static("bytes"),
        );
        return res;
    }
    if headers.get(header::RANGE).is_some() {
        let mut res = StatusCode::RANGE_NOT_SATISFIABLE.into_response();
        res.headers_mut().insert(
            header::CONTENT_RANGE,
            format!("bytes */{len}").parse().expect("content-range"),
        );
        return res;
    }
    let mut res = (
        StatusCode::OK,
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

// whole + ranged answers for files too big to read whole
async fn data_big_response(
    headers: &HeaderMap,
    full: &std::path::Path,
    len: u64,
    mtime: Option<(u64, u32)>,
) -> Response {
    let etag = data_etag(len, mtime);
    if etag_matches(headers, &etag) {
        return not_modified_response(&etag, "no-cache");
    }
    let ctype = "application/octet-stream";
    let Some(range) = data_parse_range(headers, len) else {
        if headers.get(header::RANGE).is_some() {
            // present but unsatisfiable: the honest 416
            return blob_unsatisfiable(len);
        }
        let Ok(file) = tokio::fs::File::open(full).await else {
            return (StatusCode::NOT_FOUND, "not found").into_response();
        };
        let mut res = (
            StatusCode::OK,
            [(header::CONTENT_TYPE, ctype.to_string())],
            data_stream(file, len),
        )
            .into_response();
        let hs = res.headers_mut();
        hs.insert(
            header::CONTENT_LENGTH,
            len.to_string().parse().expect("content-length"),
        );
        hs.insert(header::ETAG, etag.parse().expect("etag"));
        hs.insert(
            header::CACHE_CONTROL,
            "no-cache".parse().expect("cache-control"),
        );
        hs.insert(
            header::ACCEPT_RANGES,
            header::HeaderValue::from_static("bytes"),
        );
        return res;
    };
    let (start, end) = range;
    let window = end - start + 1;
    let body = if window <= DATA_WINDOW_MAX {
        let Ok(mut file) = tokio::fs::File::open(full).await else {
            return (StatusCode::NOT_FOUND, "not found").into_response();
        };
        if file.seek(std::io::SeekFrom::Start(start)).await.is_err() {
            return (StatusCode::INTERNAL_SERVER_ERROR, "seek failed").into_response();
        }
        let mut buf = vec![0u8; usize::try_from(window).unwrap_or(usize::MAX)];
        // a short read means the file shrank underneath us: serve what is
        // there, the client's length check rejects the truncated window
        let mut got = 0usize;
        while got < buf.len() {
            match file.read(&mut buf[got..]).await {
                Ok(0) => break,
                Ok(n) => got += n,
                Err(_) => break,
            }
        }
        buf.truncate(got);
        buf
    } else {
        let Ok(file) = tokio::fs::File::open(full).await else {
            return (StatusCode::NOT_FOUND, "not found").into_response();
        };
        // oversized range: not representable as a Vec answer - stream it,
        // the 206 framing carries the true length
        let mut res = (
            StatusCode::PARTIAL_CONTENT,
            [(header::CONTENT_TYPE, ctype.to_string())],
            data_stream(file, window),
        )
            .into_response();
        let hs = res.headers_mut();
        hs.insert(
            header::CONTENT_RANGE,
            format!("bytes {start}-{end}/{len}")
                .parse()
                .expect("content-range"),
        );
        hs.insert(
            header::CONTENT_LENGTH,
            window.to_string().parse().expect("content-length"),
        );
        hs.insert(header::ETAG, etag.parse().expect("etag"));
        hs.insert(
            header::CACHE_CONTROL,
            "no-cache".parse().expect("cache-control"),
        );
        hs.insert(
            header::ACCEPT_RANGES,
            header::HeaderValue::from_static("bytes"),
        );
        return res;
    };
    let mut res = (
        StatusCode::PARTIAL_CONTENT,
        [(header::CONTENT_TYPE, ctype.to_string())],
        body,
    )
        .into_response();
    let hs = res.headers_mut();
    hs.insert(
        header::CONTENT_RANGE,
        format!("bytes {start}-{end}/{len}")
            .parse()
            .expect("content-range"),
    );
    hs.insert(header::ETAG, etag.parse().expect("etag"));
    hs.insert(
        header::CACHE_CONTROL,
        "no-cache".parse().expect("cache-control"),
    );
    hs.insert(
        header::ACCEPT_RANGES,
        header::HeaderValue::from_static("bytes"),
    );
    res
}

// serve one resolved (or mapped) file: inline + flavor processing for small
// bodies, streamed/ranged for big ones
// serve one resolved (or mapped) file verbatim: inlined when small, streamed
// and ranged when big. An on-disk manifest (a real aops mirror's
// patchlist/patchcfg, say) is served byte-identical - no synthesis, no
// rewriting happens on mounted content.
async fn data_serve_file(headers: &HeaderMap, full: &std::path::Path) -> Response {
    let Ok(meta) = tokio::fs::metadata(full).await else {
        return (StatusCode::NOT_FOUND, "not found").into_response();
    };
    if meta.is_dir() {
        return (StatusCode::NOT_FOUND, "not found").into_response();
    }
    let len = meta.len();
    if len <= DATA_INLINE_MAX {
        let Ok(bytes) = tokio::fs::read(full).await else {
            return (StatusCode::INTERNAL_SERVER_ERROR, "read failed").into_response();
        };
        data_inline_response(headers, bytes, "application/octet-stream", "no-cache")
    } else {
        data_big_response(headers, full, len, data_mtime(&meta).await).await
    }
}

// the mount route: everything the client can ask a patch server for. On-disk
// files win (an existing manifest on disk is served as-is); the generated
// MythicMFT product lanes (patchlist product manifest, base/pkg.mft,
// base/unpacked/<name hash>) fill in a raw install folder.
async fn data_file(
    State(state): State<AppState>,
    Path((name, path)): Path<(String, String)>,
    headers: HeaderMap,
) -> Response {
    if data_dir_of(&state, &name).is_none() {
        register_shards_mounts(&state).await;
    }
    let Some(base) = data_dir_of(&state, &name) else {
        return (StatusCode::NOT_FOUND, format!("unknown data mount: {name}")).into_response();
    };
    let tail = data_strip_client_params(&path);
    if let Some(full) = data_resolve(&base, tail).await {
        // on-disk content serves as-is (including an existing manifest)
        return data_serve_file(&headers, &full).await;
    }

    // generated product lanes (a raw install folder presents as a MythicMFT
    // product; see the DataProduct notes)
    match tail {
        "patchlist" | "uo-legacyrelease.prod" => {
            // the product ladder fetches "<root><manifest>" and sniffs the
            // content: a MythicMFT product file plans through the official
            // manifest path
            let Some(product) = data_product(&state, &name).await else {
                return (StatusCode::NOT_FOUND, "empty data mount").into_response();
            };
            data_inline_response(&headers, product.product.clone(), "text/xml", "no-cache")
        }
        "base/pkg.mft" => {
            let Some(product) = data_product(&state, &name).await else {
                return (StatusCode::NOT_FOUND, "empty data mount").into_response();
            };
            data_inline_response(&headers, product.pkg.clone(), "text/xml", "no-cache")
        }
        unpacked if unpacked.starts_with("base/unpacked/") => {
            let hash = &unpacked["base/unpacked/".len()..];
            let Some(full) = data_product(&state, &name)
                .await
                .and_then(|p| p.blobs.get(hash).cloned())
            else {
                return (StatusCode::NOT_FOUND, "not found").into_response();
            };
            data_serve_file(&headers, &full).await
        }
        _ => (StatusCode::NOT_FOUND, "not found").into_response(),
    }
}

// blob-window answers for /data references (the ?u= form of the ranged
// read): straight off the mounted file (or the generated product's blob map)
// - the source is already local, so there is no second disk cache to consult.
// Same response shape as the upstream lanes (X-Blob-Total = file length) so
// the client cannot tell the difference.
async fn data_blob_window(
    state: &AppState,
    headers: &HeaderMap,
    reference: &str,
    offset: u64,
    size: u64,
) -> Response {
    let not_found = || (StatusCode::NOT_FOUND, "not found").into_response();
    let rest = reference.strip_prefix("/data/").unwrap_or(reference);
    let Some((name, tail)) = rest.split_once('/') else {
        return not_found();
    };
    if data_dir_of(state, name).is_none() {
        register_shards_mounts(state).await;
    }
    let Some(base) = data_dir_of(state, name) else {
        return not_found();
    };
    let tail = data_strip_client_params(tail);
    let mut full = data_resolve(&base, tail).await;
    if full.is_none() {
        // not on disk: the generated product's blob map answers for the
        // name-hashed loose files
        if let Some(hash) = tail.strip_prefix("base/unpacked/") {
            if let Some(product) = data_product(state, name).await {
                full = product.blobs.get(hash).cloned();
            }
        }
    }
    let Some(full) = full else {
        return not_found();
    };
    let Ok(meta) = tokio::fs::metadata(&full).await else {
        return not_found();
    };
    if meta.is_dir() {
        return not_found();
    }
    let len = meta.len();
    if offset >= len {
        return blob_unsatisfiable(len);
    }
    let start = offset;
    let end_in = offset.saturating_add(size).saturating_sub(1).min(len - 1);
    let served_len = if size == 0 { 0 } else { end_in - start + 1 };
    let mut buf = vec![0u8; usize::try_from(served_len).unwrap_or(0)];
    if served_len > 0 {
        let Ok(mut file) = tokio::fs::File::open(&full).await else {
            return not_found();
        };
        if file.seek(std::io::SeekFrom::Start(start)).await.is_err() {
            return (StatusCode::INTERNAL_SERVER_ERROR, "seek failed").into_response();
        }
        let mut got = 0usize;
        while got < buf.len() {
            match file.read(&mut buf[got..]).await {
                Ok(0) => break,
                Ok(n) => got += n,
                Err(_) => break,
            }
        }
        buf.truncate(got);
    }
    blob_window_response(
        headers,
        "application/octet-stream",
        &data_etag(len, data_mtime(&meta).await),
        offset,
        len,
        buf,
    )
}

// ---------------------------------------------------------------------------
// shards.cfg integration: `ProductUrl=local:<path>` entries are SERVER-SIDE
// references to folders this proxy can serve. They are mounted under a name
// derived from the shard's own Name, and the served shards.cfg carries the
// same-origin product url in the producturl field - the shell passes it to the
// client unchanged and the whole flow stays on this origin.
// ---------------------------------------------------------------------------

// tolerant [Shard]-section scan mirroring the page's parseShardsIni: returns
// (shard name, local product url path) pairs
fn shards_local_product_urls(text: &str) -> Vec<(String, String)> {
    let mut out: Vec<(String, String)> = Vec::new();
    let mut name: Option<String> = None;
    let mut product_url: Option<String> = None;
    let flush = |out: &mut Vec<(String, String)>,
                 name: &mut Option<String>,
                 product_url: &mut Option<String>| {
        if let (Some(n), Some(i)) = (name.take(), product_url.take()) {
            out.push((n, i));
        }
        *name = None;
        *product_url = None;
    };
    for line in text.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with(';') || line.starts_with('#') {
            continue;
        }
        if line.starts_with('[') {
            flush(&mut out, &mut name, &mut product_url);
            continue;
        }
        let Some((key, value)) = line.split_once('=') else {
            continue;
        };
        let key = key.trim().to_ascii_lowercase();
        let value = value.trim();
        match key.as_str() {
            "shardname" => name = Some(value.to_string()),
            "producturl" => {
                if let Some(path) = value.strip_prefix("local:") {
                    product_url = Some(path.trim().trim_end_matches('/').to_string());
                }
            }
            _ => {}
        }
    }
    flush(&mut out, &mut name, &mut product_url);
    out
}

// parse the root shards.cfg's LoginServer= entries: (shard name, [(host, port)]).
// "host,port" per entry, multiple servers as comma-separated pairs.
// (currently unused: the shards.cfg auto-allowlist call site is disabled)
#[allow(dead_code)]
fn shards_login_servers(text: &str) -> Vec<(String, Vec<(String, u16)>)> {
    let mut out: Vec<(String, Vec<(String, u16)>)> = Vec::new();
    let mut name: Option<String> = None;
    let mut servers: Vec<(String, u16)> = Vec::new();
    let flush = |out: &mut Vec<(String, Vec<(String, u16)>)>,
                 name: &mut Option<String>,
                 servers: &mut Vec<(String, u16)>| {
        if let (Some(n), true) = (name.take(), !servers.is_empty()) {
            out.push((n, std::mem::take(servers)));
        }
        *name = None;
        servers.clear();
    };
    for line in text.lines() {
        let line = line.trim();
        if line.is_empty() || line.starts_with(';') || line.starts_with('#') {
            continue;
        }
        if line.starts_with('[') {
            flush(&mut out, &mut name, &mut servers);
            continue;
        }
        let Some((key, value)) = line.split_once('=') else {
            continue;
        };
        let key = key.trim().to_ascii_lowercase();
        let value = value.trim();
        match key.as_str() {
            "shardname" => name = Some(value.to_string()),
            "loginserver" => {
                let fields: Vec<&str> = value.split(',').map(str::trim).collect();
                for pair in fields.chunks(2) {
                    if pair.len() != 2 {
                        continue;
                    }
                    let (host, port) = (pair[0], pair[1]);
                    if host.is_empty() {
                        continue;
                    }
                    match port.parse::<u16>() {
                        Ok(port) => servers.push((host.to_string(), port)),
                        Err(_) => eprintln!(
                            "xuoproxy: shards.cfg: bad LoginServer port for '{}' entry '{}'",
                            name.as_deref().unwrap_or("?"),
                            value
                        ),
                    }
                }
            }
            _ => {}
        }
    }
    flush(&mut out, &mut name, &mut servers);
    out
}

fn data_sanitize_mount_name(raw: &str) -> String {
    let mut out = String::with_capacity(raw.len());
    let mut last_underscore = false;
    for c in raw.chars() {
        let ok = c.is_ascii_alphanumeric() || c == '_' || c == '-';
        let c = if ok { c.to_ascii_lowercase() } else { '_' };
        if c == '_' && last_underscore {
            continue;
        }
        last_underscore = c == '_';
        out.push(c);
    }
    out.trim_matches('_').to_string()
}

// parse + register the root shards.cfg's local product urls (idempotent, cheap:
// one small file read; also self-heals /data requests that arrive before any
// shards.cfg fetch)
async fn register_shards_mounts(state: &AppState) {
    let Ok(text) = tokio::fs::read_to_string(state.root.join("shards.cfg")).await else {
        return;
    };
    for (shard_name, path) in shards_local_product_urls(&text) {
        let mut mount = data_sanitize_mount_name(&shard_name);
        if mount.is_empty() {
            mount = format!("data-{:08x}", fnv1a64(path.as_bytes()) as u32);
        }
        let resolved = if std::path::Path::new(&path).is_absolute() {
            PathBuf::from(&path)
        } else {
            // relative product urls live next to the served tree
            state.root.join(&path)
        };
        let mut mounts = state.data_dirs.lock().expect("data mounts");
        if let Some(existing) = mounts.get(&mount) {
            if existing != &resolved {
                println!(
                    "xuoproxy: shards mount '{}' already points at {}, not replacing with {}",
                    mount,
                    existing.display(),
                    resolved.display()
                );
            }
            continue;
        }
        if mounts.insert(mount.clone(), resolved.clone()).is_none() {
            println!(
                "xuoproxy: shards mount '{}' -> {}",
                mount,
                resolved.display()
            );
        }
    }
}

// the served shards.cfg: local product urls become the same-origin product url
// ("/data/<mount>/"), which the shell passes through and the client plans
// against (the generated product manifest answers at that root)
async fn serve_shards_cfg(
    state: &AppState,
    req_headers: &axum::http::HeaderMap,
) -> Option<Response> {
    let bytes = tokio::fs::read(state.root.join("shards.cfg")).await.ok()?;
    register_shards_mounts(state).await;
    let text = String::from_utf8_lossy(&bytes).into_owned();
    let mounts = |shard: &str, path: &str| -> String {
        let mut mount = data_sanitize_mount_name(shard);
        if mount.is_empty() {
            mount = format!("data-{:08x}", fnv1a64(path.as_bytes()) as u32);
        }
        format!("/data/{mount}/")
    };
    let mut out = String::with_capacity(text.len() + 64);
    let mut name = String::new();
    let mut replaced = false;
    for line in text.lines() {
        let trimmed = line.trim();
        if trimmed.starts_with('[') {
            name.clear();
        } else if let Some((key, value)) = trimmed.split_once('=') {
            match key.trim().to_ascii_lowercase().as_str() {
                "shardname" => name = value.trim().to_string(),
                "producturl" => {
                    if let Some(path) = value.trim().strip_prefix("local:") {
                        let eq = line.find('=').expect("producturl line has =");
                        out.push_str(&line[..eq + 1]);
                        out.push_str(&mounts(&name, path.trim()));
                        out.push('\n');
                        replaced = true;
                        continue;
                    }
                }
                _ => {}
            }
        }
        out.push_str(line);
        out.push('\n');
    }
    if !replaced {
        return None; // nothing local: serve the file verbatim below
    }
    let body = out.into_bytes();
    let etag = format!("\"{:016x}-{:x}\"", fnv1a64(&body), body.len());
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
        return Some(not_modified);
    }
    let mut res = ([(header::CONTENT_TYPE, "text/plain")], body).into_response();
    res.headers_mut()
        .insert(header::ETAG, etag.parse().expect("etag"));
    res.headers_mut().insert(
        header::CACHE_CONTROL,
        "no-cache".parse().expect("cache-control"),
    );
    Some(res)
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

    // the shard list is served THROUGH the proxy so its `Installer=local:`
    // entries can be mounted and rewritten to same-origin product urls
    if rel == "/shards.cfg" {
        if let Some(response) = serve_shards_cfg(&state, &req_headers).await {
            return response;
        }
    }

    let full = state.root.join(rel.trim_start_matches('/'));
    match tokio::fs::read(&full).await {
        Ok(bytes) => {
            let mime = mime_of(&full);
            // the root artifacts share filenames across builds (crossuo.js /
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

// ---------------------------------------------------------------------------
// tests: the name-hash port must match external/lookup3.h bit for bit (the
// client fetches by these names); vectors produced by the C implementation
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::lookup3_hashlittle2;

    #[test]
    fn lookup3_matches_c_reference() {
        let cases: [(&str, &str); 12] = [
            ("art.mul", "f52ecc510653c443"),
            ("map0leg.uop", "6f51007c3b82fa4b"),
            ("cliloc.enu", "bc90bd486945d920"),
            ("a", "58d68708582647ac"),
            ("ab", "fbb3a8df6b79a0f2"),
            ("abc", "0e3976313c03be9e"),
            ("abcd", "b5f4889ce20dd3fa"),
            ("abcde", "026d72de03cd18df"),
            ("abcdefabcdefabcdefghijkl", "6a87023dbbbddc78"),
            ("abcdefabcdefabcdefghijk", "8a49d082b73c25bd"),
            ("0123456789ab", "1065e50af0ec5868"),
            ("0123456789abc", "7351ce56595621ef"),
        ];
        for (name, want) in cases {
            let (ph, sh) = lookup3_hashlittle2(name.as_bytes(), 0, 0);
            assert_eq!(format!("{ph:08x}{sh:08x}"), want, "name {name}");
        }
    }

    #[test]
    fn blob_name_uses_lowercase_name() {
        assert_eq!(super::data_blob_name("art.mul"), "f52ecc510653c443");
    }
}

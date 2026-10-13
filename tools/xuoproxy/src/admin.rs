// admin.rs - internet-exposure plumbing for xuoproxy: client-IP resolution
// (behind a reverse proxy), per-IP rate limiting, persistent IP bans, usage
// stats and the SSRF guards for the client-controlled upstream lanes.

use std::collections::{HashMap, HashSet};
use std::net::{IpAddr, SocketAddr};
use std::path::PathBuf;
use std::sync::atomic::{AtomicI64, AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::time::Instant;

use axum::body::Body;
use axum::body::HttpBody;
use axum::response::Response;
use axum::extract::{Request, State};
use axum::http::{header, HeaderMap, StatusCode};
use axum::middleware::Next;
use axum::response::IntoResponse;

use crate::AppState;


// ---------------------------------------------------------------------------
// client IP: peer socket, or X-Forwarded-For when the front proxy is trusted
// ---------------------------------------------------------------------------

pub fn client_ip(req: &Request, trust_proxy: bool) -> IpAddr {
    if trust_proxy {
        if let Some(xff) = req.headers().get("x-forwarded-for").and_then(|v| v.to_str().ok()) {
            // first hop of the chain: appended by the FIRST proxy (ferron)
            // from its peer address; later entries would be client-forgeable
            if let Some(first) = xff.split(',').next() {
                if let Ok(ip) = first.trim().parse::<IpAddr>() {
                    return ip;
                }
            }
        }
    }
    req.extensions()
        .get::<axum::extract::ConnectInfo<SocketAddr>>()
        .map(|c| c.0.ip())
        .unwrap_or(IpAddr::from([0, 0, 0, 0]))
}

// ---------------------------------------------------------------------------
// ban list: in-memory rules + persisted banfile
// ---------------------------------------------------------------------------

#[derive(Clone, Debug)]
struct BanRule {
    addr: IpAddr,
    prefix: u8,
    until: u64, // unix secs; 0 = permanent
    reason: String,
}

impl BanRule {
    fn parse(line: &str) -> Option<BanRule> {
        let mut parts = line.split_whitespace();
        let spec = parts.next()?;
        let until = parts.next()?.parse().ok()?;
        let reason = parts.collect::<Vec<_>>().join(" ");
        let (addr, prefix): (IpAddr, u8) = match spec.split_once('/') {
            Some((a, p)) => (a.parse().ok()?, p.parse().ok()?),
            None => (spec.parse().ok()?, if spec.contains(':') { 128 } else { 32 }),
        };
        let max = if addr.is_ipv4() { 32 } else { 128 };
        if prefix > max {
            return None;
        }
        Some(BanRule {
            addr,
            prefix,
            until,
            reason,
        })
    }

    fn serialize(&self) -> String {
        if self.reason.is_empty() {
            format!("{} {}", self.net_string(), self.until)
        } else {
            format!("{} {} {}", self.net_string(), self.until, self.reason)
        }
    }

    fn net_string(&self) -> String {
        let max = if self.addr.is_ipv4() { 32 } else { 128 };
        if self.prefix >= max {
            self.addr.to_string()
        } else {
            format!("{}/{}", self.addr, self.prefix)
        }
    }

    fn matches(&self, ip: &IpAddr) -> bool {
        // v4 rules match v4 addresses only (no v4-mapped cross-match)
        match (self.addr, *ip) {
            (IpAddr::V4(a), IpAddr::V4(b)) => prefix_match(&a.octets(), &b.octets(), self.prefix),
            (IpAddr::V6(a), IpAddr::V6(b)) => prefix_match(&a.octets(), &b.octets(), self.prefix),
            _ => false,
        }
    }
}

fn prefix_match(a: &[u8], b: &[u8], prefix: u8) -> bool {
    let full = (prefix / 8) as usize;
    if full > a.len() || a[..full] != b[..full] {
        return false;
    }
    let rem = prefix % 8;
    if rem == 0 {
        return true;
    }
    let mask = (0xffu16 << (8 - rem)) as u8 & 0xff;
    a[full] & mask == b[full] & mask
}

struct Bucket {
    tokens: f64,
    last: Instant,
}

// ---------------------------------------------------------------------------
// usage stats
// ---------------------------------------------------------------------------

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum Lane {
    Manifest,
    Files,
    Eamythic,
    Blob,
    Stream,
    Data,
    Static,
    Ws,
    Admin,
    Other,
}

impl Lane {
    fn name(self) -> &'static str {
        match self {
            Lane::Manifest => "manifest",
            Lane::Files => "files",
            Lane::Eamythic => "eamythic",
            Lane::Blob => "blob",
            Lane::Stream => "stream",
            Lane::Data => "data",
            Lane::Static => "static",
            Lane::Ws => "ws",
            Lane::Admin => "admin",
            Lane::Other => "other",
        }
    }
    fn all() -> [Lane; 10] {
        [
            Lane::Manifest,
            Lane::Files,
            Lane::Eamythic,
            Lane::Blob,
            Lane::Stream,
            Lane::Data,
            Lane::Static,
            Lane::Ws,
            Lane::Admin,
            Lane::Other,
        ]
    }
    fn classify(path: &str) -> Lane {
        if path.starts_with("/patch/manifest/") {
            Lane::Manifest
        } else if path.starts_with("/patch/files/") {
            Lane::Files
        } else if path.starts_with("/patch/eamythic/") {
            Lane::Eamythic
        } else if path.starts_with("/blob/") {
            Lane::Blob
        } else if path.starts_with("/stream/") {
            Lane::Stream
        } else if path.starts_with("/data/") {
            Lane::Data
        } else if path.starts_with("/ws/") {
            Lane::Ws
        } else if path.starts_with("/admin") || path == "/stats" || path == "/healthz" {
            Lane::Admin
        } else if path.starts_with("/patch/") {
            Lane::Other
        } else {
            Lane::Static
        }
    }
}

struct LaneStats {
    requests: AtomicU64,
    bytes: AtomicU64,
}

pub struct Stats {
    started_at: u64,
    lanes: Vec<(Lane, LaneStats)>,
    requests_total: AtomicU64,
    bytes_total: AtomicU64,
    // status classes: [2xx, 3xx, 4xx, 429, 5xx]
    status: [AtomicU64; 5],
    cache_hits: AtomicU64,
    cache_misses: AtomicU64,
    rate_rejected: AtomicU64,
    ws_active: AtomicI64,
    ws_opened: AtomicU64,
    ws_closed: AtomicU64,
    ws_failed: AtomicU64,
    ws_bytes_up: AtomicU64,
    ws_bytes_down: AtomicU64,
    ws_duration_secs: AtomicU64,
    clients: Mutex<HashMap<IpAddr, (u64, u64)>>, // ip -> (requests, bytes)
}

impl Stats {
    fn new() -> Stats {
        Stats {
            started_at: crate::now_secs(),
            lanes: Lane::all()
                .into_iter()
                .map(|l| {
                    (
                        l,
                        LaneStats {
                            requests: AtomicU64::new(0),
                            bytes: AtomicU64::new(0),
                        },
                    )
                })
                .collect(),
            requests_total: AtomicU64::new(0),
            bytes_total: AtomicU64::new(0),
            status: Default::default(),
            cache_hits: AtomicU64::new(0),
            cache_misses: AtomicU64::new(0),
            rate_rejected: AtomicU64::new(0),
            ws_active: AtomicI64::new(0),
            ws_opened: AtomicU64::new(0),
            ws_closed: AtomicU64::new(0),
            ws_failed: AtomicU64::new(0),
            ws_bytes_up: AtomicU64::new(0),
            ws_bytes_down: AtomicU64::new(0),
            ws_duration_secs: AtomicU64::new(0),
            clients: Mutex::new(HashMap::new()),
        }
    }

    pub fn cache_hit(&self) {
        self.cache_hits.fetch_add(1, Ordering::Relaxed);
    }
    pub fn cache_miss(&self) {
        self.cache_misses.fetch_add(1, Ordering::Relaxed);
    }
    pub fn ws_open(&self) {
        self.ws_opened.fetch_add(1, Ordering::Relaxed);
        self.ws_active.fetch_add(1, Ordering::Relaxed);
    }
    pub fn ws_close(&self, duration: std::time::Duration, up: u64, down: u64) {
        self.ws_active.fetch_sub(1, Ordering::Relaxed);
        self.ws_closed.fetch_add(1, Ordering::Relaxed);
        self.ws_duration_secs
            .fetch_add(duration.as_secs(), Ordering::Relaxed);
        self.ws_bytes_up.fetch_add(up, Ordering::Relaxed);
        self.ws_bytes_down.fetch_add(down, Ordering::Relaxed);
    }
    pub fn ws_fail(&self) {
        self.ws_failed.fetch_add(1, Ordering::Relaxed);
    }
    pub fn ws_active(&self) -> i64 {
        self.ws_active.load(Ordering::Relaxed)
    }

    fn record(&self, lane: Lane, status: u16, bytes: u64, ip: Option<IpAddr>) {
        self.requests_total.fetch_add(1, Ordering::Relaxed);
        self.bytes_total.fetch_add(bytes, Ordering::Relaxed);
        if let Some((_, ls)) = self.lanes.iter().find(|(l, _)| *l == lane) {
            ls.requests.fetch_add(1, Ordering::Relaxed);
            ls.bytes.fetch_add(bytes, Ordering::Relaxed);
        }
        let class = match status {
            429 => 3,
            s if (200..300).contains(&s) => 0,
            s if (300..400).contains(&s) => 1,
            s if (400..500).contains(&s) => 2,
            _ => 4,
        };
        self.status[class].fetch_add(1, Ordering::Relaxed);
        if let Some(ip) = ip {
            let mut clients = self.clients.lock().expect("stats clients");
            // hard cap so unbounded attacker IPs cannot grow memory forever;
            // further IPs just miss per-client accounting
            if clients.len() < 8192 || clients.contains_key(&ip) {
                let e = clients.entry(ip).or_insert((0, 0));
                e.0 += 1;
                e.1 += bytes;
            }
        }
    }

    fn json(&self, admin: &Admin) -> String {
        let mut s = String::with_capacity(4096);
        s.push('{');
        let up = crate::now_secs().saturating_sub(self.started_at);
        s.push_str(&format!(
            "\"uptime_secs\":{up},\"requests_total\":{},\"bytes_total\":{},",
            self.requests_total.load(Ordering::Relaxed),
            self.bytes_total.load(Ordering::Relaxed)
        ));
        s.push_str("\"lanes\":{");
        let lanes = self
            .lanes
            .iter()
            .map(|(l, ls)| {
                format!(
                    "\"{}\":{{\"requests\":{},\"bytes\":{}}}",
                    l.name(),
                    ls.requests.load(Ordering::Relaxed),
                    ls.bytes.load(Ordering::Relaxed)
                )
            })
            .collect::<Vec<_>>()
            .join(",");
        s.push_str(&lanes);
        s.push_str("},\"status\":{");
        let st = ["2xx", "3xx", "4xx", "429", "5xx"]
            .iter()
            .zip(self.status.iter())
            .map(|(n, a)| format!("\"{}\":{}", n, a.load(Ordering::Relaxed)))
            .collect::<Vec<_>>()
            .join(",");
        s.push_str(&st);
        s.push_str(&format!(
            "}},\"cache\":{{\"hits\":{},\"misses\":{}}},",
            self.cache_hits.load(Ordering::Relaxed),
            self.cache_misses.load(Ordering::Relaxed)
        ));
        s.push_str(&format!(
            "\"rate_limit\":{{\"rejected\":{}}},",
            self.rate_rejected.load(Ordering::Relaxed)
        ));
        let closed = self.ws_closed.load(Ordering::Relaxed);
        let dur = self.ws_duration_secs.load(Ordering::Relaxed);
        let avg = if closed > 0 { dur / closed } else { 0 };
        s.push_str(&format!(
            "\"ws\":{{\"active\":{},\"opened\":{},\"closed\":{},\"failed\":{},\"bytes_up\":{},\"bytes_down\":{},\"duration_secs_total\":{},\"duration_secs_avg\":{}}},",
            self.ws_active(),
            self.ws_opened.load(Ordering::Relaxed),
            closed,
            self.ws_failed.load(Ordering::Relaxed),
            self.ws_bytes_up.load(Ordering::Relaxed),
            self.ws_bytes_down.load(Ordering::Relaxed),
            dur,
            avg
        ));
        let (bans_active, bans_total) = admin.ban_counts();
        s.push_str(&format!(
            "\"bans\":{{\"active\":{},\"total\":{}}},",
            bans_active, bans_total
        ));
        s.push_str("\"top_clients\":[");
        let clients = self.clients.lock().expect("stats clients");
        let mut top: Vec<_> = clients.iter().map(|(ip, (r, b))| (*ip, *r, *b)).collect();
        top.sort_by(|a, b| b.2.cmp(&a.2).then(b.1.cmp(&a.1)));
        top.truncate(10);
        let tops = top
            .iter()
            .map(|(ip, r, b)| format!("{{\"ip\":\"{ip}\",\"requests\":{r},\"bytes\":{b}}}"))
            .collect::<Vec<_>>()
            .join(",");
        s.push_str(&tops);
        s.push_str("]}");
        s
    }
}

// ---------------------------------------------------------------------------
// the admin bundle (lives in AppState)
// ---------------------------------------------------------------------------

pub struct Admin {
    pub trust_proxy: bool,
    pub rate_rps: u64, // 0 = rate limiting off
    pub rate_burst: u32,
    pub auto_ban_violations: u32, // 0 = auto-ban off
    pub admin_token: Option<String>,
    pub allow_private_upstream: bool,
    pub ws_allow: Option<HashSet<String>>, // None = open; Some(set) = only these host:port
    banfile: Option<PathBuf>,
    bans: Mutex<Vec<BanRule>>,
    buckets: Mutex<HashMap<IpAddr, Bucket>>,
    violations: Mutex<HashMap<IpAddr, u32>>,
    pub stats: Arc<Stats>,
}

impl Admin {
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        trust_proxy: bool,
        rate_rps: u64,
        rate_burst: u32,
        auto_ban_violations: u32,
        admin_token: Option<String>,
        allow_private_upstream: bool,
        ws_allow: Option<HashSet<String>>,
        banfile: Option<PathBuf>,
    ) -> Admin {
        let bans = banfile
            .as_ref()
            .and_then(|p| std::fs::read_to_string(p).ok())
            .map(|text| {
                let mut rules = Vec::new();
                for line in text.lines() {
                    let line = line.split('#').next().unwrap_or("").trim();
                    if line.is_empty() {
                        continue;
                    }
                    match BanRule::parse(line) {
                        Some(r) => rules.push(r),
                        None => eprintln!("xuoproxy: banfile: skipping bad line: {line}"),
                    }
                }
                println!("xuoproxy: loaded {} ban rule(s) from banfile", rules.len());
                rules
            })
            .unwrap_or_default();
        Admin {
            trust_proxy,
            rate_rps,
            rate_burst,
            auto_ban_violations,
            admin_token,
            allow_private_upstream,
            ws_allow,
            banfile,
            bans: Mutex::new(bans),
            buckets: Mutex::new(HashMap::new()),
            violations: Mutex::new(HashMap::new()),
            stats: Arc::new(Stats::new()),
        }
    }

    pub fn token_ok(&self, headers: &HeaderMap, query: Option<&str>) -> bool {
        let Some(expected) = &self.admin_token else {
            return false;
        };
        if let Some(b) = headers
            .get(header::AUTHORIZATION)
            .and_then(|v| v.to_str().ok())
        {
            let b = b.trim();
            if let Some(tok) = b
                .strip_prefix("Bearer ")
                .or_else(|| b.strip_prefix("bearer "))
            {
                return constant_time_eq(tok, expected);
            }
        }
        // browsers/webhooks cannot always set headers; accept ?token= too
        if let Some(q) = query {
            for pair in q.split('&') {
                if let Some(tok) = pair.strip_prefix("token=") {
                    return constant_time_eq(tok, expected);
                }
            }
        }
        false
    }

    pub fn banned(&self, ip: &IpAddr) -> bool {
        let now = crate::now_secs();
        let mut bans = self.bans.lock().expect("bans");
        // expired rules are pruned opportunistically
        bans.retain(|b| b.until == 0 || b.until > now);
        bans.iter().any(|b| b.matches(ip))
    }

    pub fn ban(&self, spec: &str, minutes: u64, reason: &str) -> Result<(), String> {
        let until = if minutes == 0 {
            0
        } else {
            crate::now_secs() + minutes * 60
        };
        let mut rule = BanRule::parse(&format!("{spec} {until}"))
            .ok_or_else(|| format!("bad ip/cidr spec: {spec}"))?;
        rule.reason = reason.to_string();
        let mut bans = self.bans.lock().expect("bans");
        bans.retain(|b| b.addr != rule.addr || b.prefix != rule.prefix);
        bans.push(rule);
        drop(bans);
        self.persist_bans();
        eprintln!("xuoproxy: banned {spec} ({minutes}m) reason: {reason}");
        Ok(())
    }

    pub fn unban(&self, spec: &str) -> Result<(), String> {
        let rule = BanRule::parse(&format!("{spec} 0")).ok_or("bad ip spec")?;
        let mut bans = self.bans.lock().expect("bans");
        let before = bans.len();
        bans.retain(|b| b.addr != rule.addr || b.prefix != rule.prefix);
        let removed = before != bans.len();
        drop(bans);
        self.persist_bans();
        if removed {
            Ok(())
        } else {
            Err(format!("no ban rule for {spec}"))
        }
    }

    fn persist_bans(&self) {
        let Some(path) = &self.banfile else { return };
        let bans = self.bans.lock().expect("bans");
        let text = bans
            .iter()
            .map(|b| b.serialize())
            .collect::<Vec<_>>()
            .join("\n");
        // write-then-rename so a crash mid-write cannot leave a truncated list
        let tmp = path.with_extension("tmp");
        if std::fs::write(&tmp, text)
            .and_then(|_| std::fs::rename(&tmp, path))
            .is_err()
        {
            eprintln!("xuoproxy: failed to persist banfile {:?}", path);
        }
    }

    fn ban_counts(&self) -> (usize, usize) {
        let now = crate::now_secs();
        let bans = self.bans.lock().expect("bans");
        let active = bans.iter().filter(|b| b.until == 0 || b.until > now).count();
        (active, bans.len())
    }

    pub fn bans_json(&self) -> String {
        let now = crate::now_secs();
        let bans = self.bans.lock().expect("bans");
        let items = bans
            .iter()
            .map(|b| {
                let active = b.until == 0 || b.until > now;
                format!(
                    "{{\"net\":\"{}\",\"until\":{},\"active\":{},\"reason\":{}}}",
                    b.net_string(),
                    b.until,
                    active,
                    json_string(&b.reason)
                )
            })
            .collect::<Vec<_>>()
            .join(",");
        format!("[{items}]")
    }

    pub fn ws_allowed(&self, host: &str, port: u16) -> bool {
        let Some(allow) = &self.ws_allow else {
            return true; // no allowlist configured: open
        };
        let key = format!("{host}:{port}").to_lowercase();
        let host_only = host.to_lowercase();
        allow.contains(&key) || allow.contains(&host_only)
    }

    // returns None = allowed, Some(retry_after_secs) = the 429 to return
    fn rate_limit(&self, ip: &IpAddr) -> Option<u64> {
        if self.rate_rps == 0 {
            return None;
        }
        let mut buckets = self.buckets.lock().expect("rate buckets");
        // opportunistic prune so abandoned IPs do not accumulate forever
        if buckets.len() > 65536 {
            buckets.clear();
        }
        let rps = self.rate_rps as f64;
        let now = Instant::now();
        let bucket = buckets.entry(*ip).or_insert(Bucket {
            tokens: self.rate_burst as f64,
            last: now,
        });
        let elapsed = now.duration_since(bucket.last).as_secs_f64();
        bucket.last = now;
        bucket.tokens = (bucket.tokens + elapsed * rps).min(self.rate_burst as f64);
        if bucket.tokens >= 1.0 {
            bucket.tokens -= 1.0;
            None
        } else {
            self.stats.rate_rejected.fetch_add(1, Ordering::Relaxed);
            self.note_violation(ip);
            Some(1)
        }
    }

    fn note_violation(&self, ip: &IpAddr) {
        if self.auto_ban_violations == 0 {
            return;
        }
        let mut v = self.violations.lock().expect("violations");
        let count = v.entry(*ip).or_insert(0);
        *count += 1;
        if *count == self.auto_ban_violations {
            drop(v);
            let _ = self.ban(&ip.to_string(), 0, "auto: rate-limit violations");
        } else if *count > self.auto_ban_violations * 10 {
            v.remove(ip); // keep the map bounded
        }
    }
}

fn constant_time_eq(a: &str, b: &str) -> bool {
    let (a, b) = (a.as_bytes(), b.as_bytes());
    if a.len() != b.len() {
        return false;
    }
    a.iter().zip(b.iter()).fold(0u8, |acc, (x, y)| acc | (x ^ y)) == 0
}

fn json_string(s: &str) -> String {
    let mut out = String::with_capacity(s.len() + 2);
    out.push('"');
    for c in s.chars() {
        match c {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            c if (c as u32) < 0x20 => out.push_str(&format!("\\u{:04x}", c as u32)),
            c => out.push(c),
        }
    }
    out.push('"');
    out
}

// ---------------------------------------------------------------------------
// middleware: guard (ban + rate limit) runs OUTERMOST, stats wraps bodies
// ---------------------------------------------------------------------------

fn exempt(path: &str) -> bool {
    path == "/healthz" || path == "/stats" || path.starts_with("/admin")
}

pub async fn guard(State(state): State<AppState>, req: Request, next: Next) -> Response {
    let admin = &state.admin;
    let path = req.uri().path().to_string();
    let ip = client_ip(&req, admin.trust_proxy);
    if !exempt(&path) {
        if admin.banned(&ip) {
            return (
                StatusCode::FORBIDDEN,
                [(header::CACHE_CONTROL, "no-store".to_string())],
                "banned",
            )
                .into_response();
        }
        if let Some(retry) = admin.rate_limit(&ip) {
            return (
                StatusCode::TOO_MANY_REQUESTS,
                [
                    (header::RETRY_AFTER, retry.to_string()),
                    (header::CACHE_CONTROL, "no-store".to_string()),
                ],
                "rate limited",
            )
                .into_response();
        }
    }
    next.run(req).await
}

pub async fn stats_mw(State(state): State<AppState>, req: Request, next: Next) -> Response {
    let lane = Lane::classify(req.uri().path());
    let ip = client_ip(&req, state.admin.trust_proxy);
    let res = next.run(req).await;
    let status = res.status().as_u16();
    let stats = state.admin.stats.clone();
    let (parts, body) = res.into_parts();
    // known-length bodies (most of them: cache serves, statics, windows) are
    // recorded immediately; streamed bodies are wrapped so the real served
    // byte count is recorded when the body finishes (or is dropped)
    if let Some(exact) = body.size_hint().exact() {
        stats.record(lane, status, exact, Some(ip));
        Response::from_parts(parts, body)
    } else {
        let rec = Arc::new(Rec {
            stats,
            lane,
            status,
            ip: Some(ip),
            bytes: AtomicU64::new(0),
            done: std::sync::atomic::AtomicBool::new(false),
        });
        let counted = CountedBody {
            inner: body,
            rec: rec.clone(),
        };
        // ensure the stat exists even for an empty streamed body
        Response::from_parts(parts, Body::new(counted))
    }
}

struct Rec {
    stats: Arc<Stats>,
    lane: Lane,
    status: u16,
    ip: Option<IpAddr>,
    bytes: AtomicU64,
    done: std::sync::atomic::AtomicBool,
}

impl Rec {
    fn finish(&self) {
        if !self.done.swap(true, Ordering::Relaxed) {
            self.stats
                .record(self.lane, self.status, self.bytes.load(Ordering::Relaxed), self.ip);
        }
    }
}

// wraps a streamed body, counts data frames and records the stat exactly
// once, when the body completes or the response is dropped/aborted
struct CountedBody {
    inner: Body,
    rec: Arc<Rec>,
}

impl http_body::Body for CountedBody {
    type Data = axum::body::Bytes;
    type Error = axum::Error;

    fn poll_frame(
        mut self: std::pin::Pin<&mut Self>,
        cx: &mut std::task::Context<'_>,
    ) -> std::task::Poll<Option<Result<http_body::Frame<Self::Data>, Self::Error>>> {
        match std::pin::Pin::new(&mut self.inner).poll_frame(cx) {
            std::task::Poll::Ready(Some(Ok(frame))) => {
                if let Some(data) = frame.data_ref() {
                    self.rec.bytes.fetch_add(data.len() as u64, Ordering::Relaxed);
                }
                std::task::Poll::Ready(Some(Ok(frame)))
            }
            std::task::Poll::Ready(None) => {
                self.rec.finish();
                std::task::Poll::Ready(None)
            }
            other => other,
        }
    }

    fn size_hint(&self) -> http_body::SizeHint {
        self.inner.size_hint()
    }
}

impl Drop for CountedBody {
    fn drop(&mut self) {
        self.rec.finish();
    }
}

// ---------------------------------------------------------------------------
// admin endpoints (token-gated; disabled entirely without --admin-token)
// ---------------------------------------------------------------------------

fn admin_disabled() -> Response {
    (
        StatusCode::NOT_FOUND,
        "admin api disabled (start with --admin-token to enable)",
    )
        .into_response()
}

fn json_response(body: String) -> Response {
    (
        StatusCode::OK,
        [(
            header::CONTENT_TYPE,
            "application/json; charset=utf-8".to_string(),
        )],
        body,
    )
        .into_response()
}

pub async fn stats_handler(
    State(state): State<AppState>,
    headers: HeaderMap,
    axum::extract::RawQuery(query): axum::extract::RawQuery,
) -> Response {
    if !state.admin.token_ok(&headers, query.as_deref()) {
        return admin_disabled();
    }
    json_response(state.admin.stats.json(&state.admin))
}

pub async fn bans_handler(
    State(state): State<AppState>,
    headers: HeaderMap,
    axum::extract::RawQuery(query): axum::extract::RawQuery,
) -> Response {
    if !state.admin.token_ok(&headers, query.as_deref()) {
        return admin_disabled();
    }
    json_response(state.admin.bans_json())
}

pub async fn ban_handler(
    State(state): State<AppState>,
    headers: HeaderMap,
    axum::extract::RawQuery(query): axum::extract::RawQuery,
) -> Response {
    ban_op(state, headers, query, true).await
}

pub async fn unban_handler(
    State(state): State<AppState>,
    headers: HeaderMap,
    axum::extract::RawQuery(query): axum::extract::RawQuery,
) -> Response {
    ban_op(state, headers, query, false).await
}

async fn ban_op(
    state: AppState,
    headers: HeaderMap,
    query: Option<String>,
    add: bool,
) -> Response {
    if !state.admin.token_ok(&headers, query.as_deref()) {
        return admin_disabled();
    }
    let mut ip = None;
    let mut minutes = 0u64;
    let mut reason = String::new();
    if let Some(q) = query.as_deref() {
        for pair in q.split('&') {
            let (k, v) = pair.split_once('=').unwrap_or((pair, ""));
            match k {
                "ip" => ip = Some(urldecode(v)),
                "minutes" => minutes = v.parse().unwrap_or(0),
                "reason" => reason = urldecode(v),
                _ => {}
            }
        }
    }
    let Some(ip) = ip else {
        return (StatusCode::BAD_REQUEST, "missing ?ip=<addr|cidr>").into_response();
    };
    let result = if add {
        state.admin.ban(&ip, minutes, &reason)
    } else {
        state.admin.unban(&ip)
    };
    match result {
        Ok(()) => json_response(format!("{{\"ok\":true,\"ip\":\"{}\"}}", json_string(&ip))),
        Err(e) => (StatusCode::BAD_REQUEST, e).into_response(),
    }
}

fn urldecode(s: &str) -> String {
    let bytes = s.replace('+', " ").into_bytes();
    let mut out = Vec::with_capacity(bytes.len());
    let mut i = 0;
    while i < bytes.len() {
        if bytes[i] == b'%' && i + 2 < bytes.len() {
            if let (Some(h), Some(l)) = (
                (bytes[i + 1] as char).to_digit(16),
                (bytes[i + 2] as char).to_digit(16),
            ) {
                out.push(((h << 4) | l) as u8);
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
// SSRF guards for the client-controlled upstream lanes (/stream, /blob?u=)
// ---------------------------------------------------------------------------

fn ip_is_public(ip: IpAddr) -> bool {
    match ip {
        IpAddr::V4(v4) => {
            !(v4.is_loopback()
                || v4.is_private()
                || v4.is_link_local()
                || v4.is_unspecified()
                || v4.is_broadcast())
        }
        IpAddr::V6(v6) => {
            if let Some(v4) = v6.to_ipv4_mapped() {
                return ip_is_public(IpAddr::V4(v4));
            }
            let seg = v6.segments();
            !(v6.is_loopback()
                || v6.is_unspecified()
                || (seg[0] & 0xfe00) == 0xfc00 // unique local fc00::/7
                || (seg[0] & 0xffc0) == 0xfe80) // link local fe80::/10
        }
    }
}

// resolve + check every address the name maps to; called right before the
// upstream fetch so a DNS rebinding to a private address is rejected too.
// Err((403, ..)) = policy denial; Err((502, ..)) = upstream/DNS failure.
pub async fn upstream_url_allowed(state: &AppState, url: &str) -> Result<(), (u16, String)> {
    if state.admin.allow_private_upstream {
        return Ok(());
    }
    let parsed = reqwest::Url::parse(url).map_err(|_| (400u16, "bad upstream url".to_string()))?;
    if parsed.scheme() != "http" && parsed.scheme() != "https" {
        return Err((403, "only http(s) upstreams are allowed".to_string()));
    }
    let host = parsed
        .host_str()
        .ok_or_else(|| (400, "upstream url has no host".to_string()))?
        .trim_start_matches('[')
        .trim_end_matches(']')
        .to_string();
    // literal IPs need no resolution
    if let Ok(ip) = host.parse::<IpAddr>() {
        if !ip_is_public(ip) {
            return Err((
                403,
                "private/loopback upstreams are not allowed on a public listener".into(),
            ));
        }
        return Ok(());
    }
    let addrs = tokio::net::lookup_host((host.as_str(), 80))
        .await
        .map_err(|e| (502u16, format!("upstream host resolution failed: {e}")))?;
    for addr in addrs {
        if !ip_is_public(addr.ip()) {
            return Err((
                403,
                format!("upstream host {host} resolves to a private address"),
            ));
        }
    }
    Ok(())
}

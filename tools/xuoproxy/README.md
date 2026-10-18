# xuoproxy

A single Rust binary that lets you serve the CrossUO web client and your
shard's game data from your own website. One process does it all:

1. **Static hosting** of the wasm build (default `html/`) with the
   `Cross-Origin-Opener-Policy` / `Cross-Origin-Embedder-Policy` headers the
   pthread runtime requires (SharedArrayBuffer).
2. **Same-origin reverse proxy** for streaming hosts, disk-cached:
   - `/patch/manifest/<path>` -> upstream manifest repo
   - `/patch/files/<path>`    -> upstream file repo
   - `/patch/eamythic/<path>` -> upstream aops root
   - `/stream/<url>`          -> any absolute http(s) url
   - `/blob/<urlhash>/<offset>/<size>?u=<url>` -> cacheable ranged reads
3. **WebSocket-to-TCP bridge** at `/ws/<host>/<port>` — the web client cannot
   open raw TCP sockets, so it connects here and the bridge pipes binary
   websocket frames to your shard's TCP stream (and back).
4. **Local data mounts** at `/data/<name>/<path>` — serve your shard's client
   data files straight from a folder on the proxy host.

## Quick start

```sh
cargo build --release
./target/release/xuoproxy --listen 127.0.0.1:8777 --root html/
```

Point your website's reverse proxy (ferron/nginx/...) at `127.0.0.1:8777` and
serve it over HTTPS. See `conf/ferron.example.conf` for a production config
(keep the COOP/COEP headers coming from xuoproxy itself — it sets them).

The web client boots against this server out of the box: its default product
url points at `/patch/manifest/...` (proxied), and shard logins go through
`/ws/<host>/<port>`.

## Serving your shard's data

Two options:

### Option A: local folder mount (recommended)

Keep the original client install on the proxy host. When a mounted folder has
no manifest on disk, xuoproxy **synthesizes a MythicMFT/MYP product** for it:
a generated `uo-legacyrelease.prod` manifest, a `base/pkg.mft` sub-manifest
listing the folder walk (same file filter the client applies), and loose-file
blobs. The product carries a content fingerprint, so a changed folder
re-plans clients on their next boot. An existing manifest on disk (e.g. a real
mirror's patch list) is always served byte-identical instead.

Two ways to mount:

```sh
# Explicit: repeatable, one mount per shard (hub deployments expose many)
xuoproxy --data myshard=/srv/uo/client --data othershard=/srv/uo/client2

# Via shards.cfg: ProductUrl=local:<path> entries are auto-mounted under a
# name derived from the shard's [Section] Name, and the served shards.cfg
# rewrites them to the same-origin /data/<name>/ url — clients need nothing
# beyond --root.
```

`shards.cfg` example (in the `--root` tree):

```ini
[MyShard]
ShardName=MyShard
ProductUrl=local:/srv/uo/client
LoginServer=play.example.com:2593
```

A remote CDN `ProductUrl=` keeps working unchanged. Relative `local:` paths
resolve against the `--root` tree.

### Option B: pre-built static product

Generate an unpacked MYP product folder once, then host it on **any** static
web server (no xuoproxy needed for the data at all):

```sh
xuoproxy --generate-product /srv/uo/client --generate-url /data/uodata/
# produces uo-legacyrelease.prod, base/pkg.mft, base/unpacked/<name hash>
```

Point `ProductUrl=` (or the client's product url) at wherever you host the
folder.

## Shard connectivity

The client connects to your shard through `/ws/<host>:<port>`. The shard
address comes from the login server list, so any shard works.

Lock it down in production:

```sh
xuoproxy --ws-allow play.example.com:2593 --ws-allow backup.example.com:2593
```

Without an allowlist on a public bind, bridges stay open (a startup warning
is printed).

## Internet exposure hardening

All of this is off/loose on the default loopback dev bind — turn it on when
facing the internet:

| Flag | Purpose |
|---|---|
| `--trust-proxy` | Take the client IP from `X-Forwarded-For` (set by your front proxy). Required behind ferron/nginx, or the rate limiter bans the proxy's own IP. |
| `--rate-limit <rps>` / `--rate-burst <n>` | Per-IP token bucket, 429 with `Retry-After`. Note: one cold client boot fires ~1700 blob window GETs — keep the burst generous. |
| `--auto-ban-violations <n>` | Ban IPs after N rate-limit hits. |
| `--banfile <path>` | Persistent ban list (`ip[/prefix] until reason` per line), rewritten on every runtime ban/unban. |
| `--admin-token <secret>` | Gates `/stats` and `/admin/*` with a Bearer token (or `?token=`); without it they are 404. |
| `--allow-private-upstream` | Let `/stream` and `/blob` fetch private addresses. On a public bind they refuse private/loopback/link-local upstreams (SSRF protection) unless this is set. |

## Admin API

With `--admin-token` set:

- `GET /stats` — JSON report: per-lane request/byte counters, status classes,
  cache hit/miss, rate-limit rejections, WS bridge gauges, ban counts,
  top-10 clients by traffic.
- `POST /admin/ban?ip=<ip>&minutes=<n>&reason=<text>`
- `POST /admin/unban?ip=<ip>`
- `GET /admin/bans`
- `GET /healthz` returns `ok` for frontend health checks.

## Other useful flags

```text
--cache-dir <dir>          on-disk cache dir (default xuoproxy-cache)
--cache-ttl <secs>         freshness of immutable cached entries (0 = forever)
--manifest-ttl <secs>      secs between upstream revalidation of manifest
                           entries (0 = every request; manifests are KBs)
--upstream-timeout <secs>  abort fetch after this much read inactivity
                           (default 300; not a total deadline)
--no-cache                 bypass the disk cache entirely
--manifest-upstream <url>  manifest repo base url (default: EA patch CDN)
--files-upstream <url>     file repo base url (default: EA CDN)
--aops-upstream <url>      aops patchlist/patchcfg root (default: uniteuo.com)
```

Cached manifests are revalidated with conditional GETs; when one actually
changes, the content entries the old body referenced are invalidated so
changed data re-fetches fresh. Rewritten resource urls carry a `?v=<token>`
version token for browser cache busting; the token is stripped before any
cache key or upstream url is computed.

## Fronting with ferron/nginx

Keep the essentials from `conf/ferron.example.conf`:

- Replace (not append) `X-Forwarded-For` with the real client IP so it cannot
  be spoofed past the rate limiter.
- Run xuoproxy with `--trust-proxy`.
- WebSocket upgrades on `/ws/...` proxy transparently — no extra directive.
- Optionally IP-allowlist `/stats` and `/admin` at the edge in addition to
  the token gate.

## Example: shard owner self-hosting

The minimal end-to-end setup for a shard owner who wants a single web client
that boots straight into their shard, served from their own domain.

Layout on the server:

```text
/srv/xuo/
├── html/                  # the wasm client build (from the CrossUO build)
│   └── shards.cfg         # your shard.cfg single entry with your shard settings
├── xuoproxy               
└── xuoproxy-cache/        # created automatically
/srv/uo/data/              # a full original client install (map*.mul, art.mul, ...)
```

`/srv/xuo/html/shards.cfg`:

```ini
[MyShard]
ShardName=MyShard
ProductUrl=local:/srv/uo/data
LoginServer=login.example.com,2593
```

No `--data` flag is needed: the `local:` product url auto-mounts the client
install under `/data/myshard/`, and xuoproxy rewrites the served shards.cfg
to the same-origin product url, so the client plans its data against your
own server. The synthesized MYP product is generated from the folder on the
fly — no patching or diffing tools required.

Run it:

```sh
cd /srv/xuo
./xuoproxy \
    --listen 127.0.0.1:8777 \
    --root html/ \
    --trust-proxy \
    --ws-allow login.example.com:2593 \
    --ws-allow games.example.com:5555 \ # your relay/game server if different
    --rate-limit 50 --rate-burst 2000 \
    --admin-token "$(cat /etc/xuoproxy-token)" \
    --banfile /srv/xuo/bans.txt
```

Then front it with ferron/nginx over HTTPS (see
`conf/ferron.example.conf`): everything on `https://play.example.com/` goes
to `127.0.0.1:8777`, with `X-Forwarded-For` replaced by the real client IP.

What players get:

- `https://play.example.com/` — the client boots in the browser without server picker.
- Data files stream from `/data/myshard/...` (same origin, no CDN needed);
  big `.uop` files are refined into ranged block fetches automatically.
- Game traffic tunnels through `/ws/play.example.com/2593` to your shard's
  TCP listener. `--ws-allow` matches the `LoginServer=` line and your game server, so 
  that is the only address the bridge will connect to.

Notes for the single-shard case:

- `--rate-burst 2000` is deliberate: a cold boot fires ~1700 blob window
  GETs up front.
- Restart xuoproxy after editing shards.cfg — the ws allowlist is fixed at
  boot; a changed shards.cfg is only re-scanned (for product mounts) when
  the file is served.

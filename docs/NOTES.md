# Implementation Notes

## T1 — Shard/link/product fields in g_Config (2026-10-04)
- New fields: `DataProductUrl`, `ShardWeb/Account/Help/Video/Credits/Discord` (white_label defaults; `ShardDiscord` default-empty). Keys: `producturl`, `shardweb`, `shardaccount`, `shardhelp`, `shardvideo`, `shardcredits`, `sharddiscord`.
- Save-back writes a field only when it differs from its white_label default (ShardDiscord: only when non-empty) so defaults never bake into crossuo.cfg and future shard switches aren't fought by stale values.
- Empty-value asymmetry (intentional): `TextFileParser` drops empty tokens, so `shardweb=` (size==1) clears the field via a pre-switch scoped to the 7 new keys; a bare key line with no `=` also clears (pre-existing keys keep defaults in that case). Left as-is: key present ⇒ value empty.
- Pre-existing, out of scope: `,` is a cfg value delimiter (string values truncate at first comma); save-key casing (`ProductUrl=`) differs from load keys (`producturl`) — matches `ShardName=` precedent, load is case-insensitive.

## T2 — Runtime consumers + ApplyShardProfile (2026-10-04)
- `ApplyShardProfile(shard_profile)` is the single shard-apply entry (src/Config.cpp:850): links empty → white_label default reset (deterministic across shard switches); product url set only when non-empty; credentials + CustomPath untouched via delegation to ApplyClientProfile; restart semantics preserved.
- `character` maps to `g_PacketManager.AutoLoginNames`, not g_Config; ApplyShardProfile round-trips the current value byte-exactly (empty stays empty — no bogus "|" entry).
- Streaming product chain: CLI --streaming-product > config (DataProductUrlSet && non-empty; EMSC: relative→origin; trailing / → append uo-legacyrelease.prod = vfs VFS_PRODUCT_FILE_DEFAULT) > platform defaults untouched (desktop empty → vfs fallback; web → XUO_WEB_PRODUCT_URL).
- Known follow-ups: ApplyShardProfile has no callers until T4 (launcher/embedded bridge) — revisit adding a debug assert for empty profile.name then.

## T3 — Launcher strip: menu bar, Updates/Backups, updater, assist, self-relaunch (2026-10-04)
- cfg_launcher.h keeps `check_updates`/`auto_update`/`enable_assist` (and `auto_close`) as parse-only compat fields: cfg_loader is strict (unknown key aborts the whole ini parse, dropping subsequent [Account] sections), so removing fields would corrupt existing user configs. Fields are unread.
- `xuo_updater.*` survives only in the `xuoi` target (`--xuo`). The boot-time changelog download lived in `xuo_init` (xuo_changelog_load) — gone with the updater.
- Self-relaunch copy-to-`xuolauncher_` removed; release builds start directly.
- `XUOL_AGENT_NAME` moved from xuo_updater.h to per-host defines (xuol_app.cpp, xuolauncher_main.cpp); embedded host keeps its own LAUNCHER_TITLE.
- Follow-up folded into T4: drop orphaned `#include <external/process.h>` in xuolauncher_main.cpp. `XUO_DEFAULT_SHARD_CHANGELOG` (white_label.h) now has zero users — kept as white-label surface for a future changelog entry.

## T4 — Launcher as shard picker: shards-first + click-to-launch (2026-10-04)
- Shards = home tab with Play button + double-click launch; account NOT required (credentials zeroed in the shard profile; login screen handles auth). Launcher closes only on successful spawn (also fixed for the legacy account-launch path, incl. missing `return` after empty-bin error).
- Shard → client profile: `shard_fill_entry` maps identity + installer→producturl + links into the extended `crossuo::entry` schema; keys omitted (never written empty) when the shard doesn't provide them. Discord matched by exact urlother token (strip "Goto: " prefix, strcasecmp) — not substring.
- Embedded: `xuol::client_apply_shard` bridge → `ApplyShardProfile` (applied→hide launcher; needs_restart→latched reload, no quit). `xuol::shard_profile` (xuol_app.h) is intentionally a distinct mirror of Config's `::shard_profile` — TU boundary (launcher doesn't compile src/Config.cpp).
- Known limitations (accepted): empty clienttype/version in a shard = "keep current" on live switch (inherited ApplyShardProfile contract; shardchk requires clientversion so exposure is clienttype only); shard launch cfgs share the profile-name namespace with account profiles (a shard named like a profile overwrites that cfg); `installer = xuo+latest` nonsense input would write `producturl=latest` (parser-level, pre-existing).
- UI automation note: XTEST pointer no-ops in this VM; verify window content via `import -window <id>` + stddev (x11grab captures black for GL windows).

## T6 — shardchk hardening + schema fix (2026-10-04)
- `protocolversion` was declared under section `account` (copy-paste) — with the shard table's strict parse it made ANY `[Shard] protocolversion` line abort the whole file; now `shard`. `[Account] protocolversion` was silently ignored before, so no published data regressed.
- shardchk now validates: non-empty url/urlforum/urlregister = http(s); urlother name+url backstop; installer per-type (rejects `xuo+latest`, requires http(s) urls); loginserver port shape = WARN only (host-only entries keep passing).
- Auto-account filter in ui_shards confirmed CORRECT as originally written (my earlier plan note claiming inversion was wrong): select_aac skips shards that HAVE urlregister → shows true auto-account shards. Comment added.
- Known nits: malformed urlother double-reports (converter + backstop); `,2593` (empty host) passes the warn check; `find("+latest")` substring test is safe today, could be `raw == "xuo+latest"` for clarity.

## T7 — Web pre-boot shard picker (2026-10-04)
- shell.html (ships as index.html; not an emscripten --shell-file) now gates crossuo.js behind bootClient(): fetch shards.cfg (same-origin → GitHub raw → ?shards=<url> override), minimal picker, or ?shard=<name|index> deep-link boots directly; every failure path falls back to today's immediate boot.
- v1 mapping: name→--shard (sanitized), loginserver→--host/--port (1-65535), installer uo+/xuo+ url→--streaming-product. Not passed (no CLI surface yet): clientversion/crypt/links — crossuo.cfg-in-IDBFS is the future carrier; client-side ?shard= resolution is the v2 shape.
- ?shard-address/?shard-port testing overrides deliberately win (appended last; popts single-option push_back replaces = last-wins, verified in external/popts.h).
- Parser: ES5, tolerant (CRLF/CR, comments, nameless sections skipped), BOM stripped, remote text rendered via textContent only.
- Known follow-ups: generic k=v URL passthrough pushes junk positionals (pre-existing); duplicate shard names resolve first-wins (documented in code); showPicker lacks a try/catch fallback boot (NIT).

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

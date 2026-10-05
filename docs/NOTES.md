# Implementation Notes

## T1 — Shard/link/product fields in g_Config (2026-10-04)
- New fields: `DataProductUrl`, `ShardWeb/Account/Help/Video/Credits/Discord` (white_label defaults; `ShardDiscord` default-empty). Keys: `producturl`, `shardweb`, `shardaccount`, `shardhelp`, `shardvideo`, `shardcredits`, `sharddiscord`.
- Save-back writes a field only when it differs from its white_label default (ShardDiscord: only when non-empty) so defaults never bake into crossuo.cfg and future shard switches aren't fought by stale values.
- Empty-value asymmetry (intentional): `TextFileParser` drops empty tokens, so `shardweb=` (size==1) clears the field via a pre-switch scoped to the 7 new keys; a bare key line with no `=` also clears (pre-existing keys keep defaults in that case). Left as-is: key present ⇒ value empty.
- Pre-existing, out of scope: `,` is a cfg value delimiter (string values truncate at first comma); save-key casing (`ProductUrl=`) differs from load keys (`producturl`) — matches `ShardName=` precedent, load is case-insensitive.

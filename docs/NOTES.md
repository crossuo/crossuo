<!--
SPDX-FileCopyrightText: 2026 CrossUO Team
SPDX-License-Identifier: AGPL-3.0-or-later
-->

# NOTES — deferred decisions and known limitations (M2)

## TLS verification is deferred

`vfs_fetch` runs with `CURLOPT_SSL_VERIFYPEER 0`, mirroring the four existing
sites in `xuocore/http.cpp`. The official product/file repos are plain http
today, so nothing is verified in practice yet; https mirrors would currently be
exposed to man-in-the-middle substitution of blobs.

Closure options (pick when the first https mirror lands):

- optional `.meta` sha256 sidecar verification of fetched parts, which
  authenticates content regardless of transport; or
- a CA-bundle strategy (ship/bundle roots, enable `CURLOPT_SSL_VERIFYPEER` +
  `CURLOPT_CAINFO`) for https repos.

## Tier refinement: anim.idx vs anim.mul

`anim.idx` (1.7 MB) is T1: LoadCommon requires it, so it is fetched during
boot (sized at boot; valid-but-unfetched is enough). `anim.mul` (190 MB legacy
fallback) stays T2: the boot
skeleton prepares it valid-but-unfetched so LoadCommon passes, and content
converges via the idle tier (T3).

## Product manifest staleness

CLOSED: an online boot refreshes the product manifest every
boot (it is ~1.6 KB; a local `--product` file still wins and offline runs keep
the cached copy), so a re-signed or re-serialized manifest is seen on the next
start. A failed refresh falls back to the cached copy with a warning.

## Embedded T1 tables: converter TODO

CLOSED: `vuoi --embed-record=<file.csv>` converts a recorder CSV into
the generated `vfs_manifest_embedded.cpp` on stdout (SPDX header, first-access
order, malformed/duplicate rows skipped with warnings). Deployment flow:
record a run, feed the CSV to the converter, replace
`xuocore/vfs_manifest_embedded.cpp` with the output and rebuild. Tests inject
converted tables through `vfs_manifest_embedded_set` (see
tests/vfs/test_update.cpp); that seam survives the deployment rewrite.

# NOTES — deferred decisions and known limitations (M3)

## MUL/loose streaming is whole-file on first fault

`vfs_trap` resolves loose files (and legacy MULs served as loose plan files)
by fetching and materializing the whole file on the first fault (the
whole-file fallback). Per-index-range streaming (`.idx` entry ranges, batched
64 KB GETs) is deferred: it needs the idx-range covering map plus range-GET
coalescing, and whole-file already unblocks map/statics/anim reads for the
common (smaller) MULs. Statics/texmaps are T1 (early background) anyway, so
the files that would benefit most from ranges are typically complete before play.

## Async-signal-safety corners of the fault path (pragmatic by design)

The POSIX `SIGSEGV` handler resolves faults synchronously and deliberately
takes a normal mutex (`s_mutex`), waits on a condvar for in-flight dedup, and
performs a blocking curl fetch *inside the handler*. Everything else in the
hot path is syscall- or lock-free: mmap/mprotect/pwrite plus preallocated
atomic counters, no logging, no allocation (fixed-size claim batches). The
alternatives (deferring resolution to a dedicated thread per fault, or a
fully lock-free registry) trade correctness for latency in acceptable,
documented ways; `--streaming-trap=off` remains the escape hatch.

Windows uses a first-chance VEH (`EXCEPTION_ACCESS_VIOLATION` only,
`EXCEPTION_CONTINUE_SEARCH` otherwise) with the same resolution core.

## Zero-page degradation (historical: before per-kind stubs)

A failed resolution zero-fills the still-protected sub-ranges of the faulting
page (entry-table and present-block bytes are preserved) and keeps the game
running with zeros - zlib/stored readers degrade naturally (GetData returns
empty / zeros). Per-kind stub payloads supersede this degradation with real
placeholder content and mark
pages `vfs_stubbed`; the page mark (`vfs_trap_page_mark_at`) is
the placeholder signal. Note the state DB intentionally keeps
`vfs_pending` for missing blobs (they stay on the fetch list); only truly
unusable blobs become `vfs_failed`.

## Failed-block retry semantics

A block whose resolution failed is not retried within the same resolution, and
a page that degraded to a zero-fill does NOT re-attempt on the next touch:
zeroed pages are readable, so touching them raises no fault (healing is not
spontaneous). Healing happens through the explicit pre-resolve paths
(`GetData`/`GetRaw`/`GetMeta` → `vfs_ensure_range`, which re-run resolution for
the range) and through the boot/idle fetch tiers; when the last absent block of
a zeroed page lands, the page mark promotes `zeroed` → `real` (the MAP_SHARED
mapping already shows the real bytes - exercised by
`trap_zeroed_page_heals`). Pages that are still protected (blocks pending) do
re-attempt on the next fault, at the cost of one fast 404 per access for
permanently missing blobs online. Stub marking (the page becomes readable)
stops the retry storm.

## EOF-tail reads crash instead of returning zeros

Managed mappings span `ceil(size / page)` pages, but only `[0, size)` is
registered: a read in `[size, page-round-up)` (the tail padding of the last
page) faults, is rejected by the containment check (`offset >= size`) and
chains to the default action - the process crashes with SIGSEGV. This is
deliberate: such a read is an index/entry-table bug, and before the trap it
would have read past the file end anyway; an earlier iteration briefly served
those reads as
zeros, which masked the bug. Containment itself is covered by test (f).

## Trap shutdown does not unmap

`vfs_trap_shutdown` (called first in `vfs_shutdown`) uninstalls the handler
and forgets the registry but leaves the mappings alive: the game may still
hold `CMappedFile` base pointers at exit and unmaps them afterwards through
the plain-`fs_unmap` fallback. Unmapping there instead would risk
use-after-unmap during teardown for no benefit (the OS reclaims at exit).

Resolutions in flight get a bounded 5s grace; mappings still resolving after
that (or after an `vfs_trap_unmap` that timed out on the same wait) turn
*zombie*: the mapping object and its pages stay mapped and registered for the
process lifetime so no resolver can be left holding freed state. Zombies are
logged loudly and leak by design - the mapping memory deliberately leaks at
shutdown rather than risking a use-after-free/use-after-unmap.

# NOTES — deferred decisions and known limitations (M4)

## Stub payload model

Stubs are deterministic pure functions of (kind, ul, is_uop, offset): a small
header followed by zeros (the map block repeats one 196-byte `MAP_BLOCK`
pattern instead). Two constraints shape every generator:

- **Stubs live inside the block's planned stored span** (`ml + ul`): the
  sparse layout reserves exactly the materialized size - packs space blocks
  by the decompressed payload and write stored entries (`Flags=0`,
  `CompressedSize=ul`), so raw-pointer readers (art, map blocks) see stored
  bytes like EA's own local archives; the compressed blob only ever exists
  in the blob store and inflates right before the block write. The stub
  entry carries the same planned shape, which pads the tail with zeros via
  the `memcpy` path of `DecompressBlock` - exactly the stub's semantics. The
  entry keeps the plan's `Hash`/`DecompressedSize`. Because stub and real
  entries now share one shape, `vfs_store_pack_asset_present` separates them
  by content: the stored head must match the blob's decompressed head
  whenever a store root is available (see "Fixed in review", the reconcile
  item), and stub entries are never promoted to fetched by reconcile.
- **Headers are all-or-nothing.** A truncated header would let the game's
  parsers walk bytes beyond the stub inside the mapped file (run-length
  overflows in `GetArtPixels`/`GetGumpPixels`). Below `vfs_stub_min_payload`
  the generator writes nothing and the plain zero-fill serves instead - those
  readers reject zeros safely (bad width / bad bounds warnings). The gump
  header short-circuits parsing entirely: its UOP width/height stay `0x0`
  (fixed in review, C1 - `UopReadIndexFile` bakes them into `CIndexObject`,
  so an invented `1x1` would overflow a healed re-decode), and
  `GetGumpPixels` warns and returns empty on `blocksize == 0`; the
  hot-swap refresh re-reads the real dims from the healed payload. The anim
  header squeezes `Offset` so the fixed-offset frame+palette+frame-info read
  stays inside the block (below 536 bytes the anim stub degrades to zeros:
  the reader walks a heap scratch buffer with fixed offsets and would
  overread - a pre-existing reader fragility for absurdly small blocks, not
  a file-format case that occurs).

UOP gumps are the only kind whose layout depends on `is_uop` (width/height
live in the payload's first 8 bytes, which `UopReadIndexFile` reads and then
skips). The stub writes `0x0` there; `vfs_refresh_index` re-reads the real
dims from `Address - 8` once the asset healed (see "Fixed in review" C1).

## Stubbed-flag mechanics and the refresh contract

- `CIndexObject::Stubbed` sits in the struct's tail padding (56-byte body on
  64-bit, 6 pad bytes before it); no size drift on either word size.
- The flag is set at **index build** from the state DB (per pack file:
  `vfs_file_state_base(uopFile.Start)` resolves the plan file once and
  `vfs_asset_stubbed_at(base, i)` answers per entry in `UopReadIndexFile`;
  `vfs_file_is_stubbed(base)` marks whole loose files once per file in
  `MulReadIndexFile`) - so both modes mark it, and it survives reboots
  (state.bin + journal op `B`). A stubbed state never seals a file and never
  satisfies `vfs_state_reconcile`; heal-to-fetched happens only through a
  real block write.
- The refresh itself is trap-driven: `vfs_refresh_index(io, DestroySprite)`
  checks `io->Stubbed` + `vfs_refresh_block(io->Address)`
  (`vfs_trap_asset_healed`: asset stubbed **and** present), calls the
  game-supplied teardown, clears `UserData`/flag, and the caller falls
  through to the normal load. One-liners live in `ExecuteGump/LandArt/
  StaticArt/Texture/Light`, `PlaySoundEffect` (local `DestroySoundCache`
  mirrors the GC's sound teardown) and `AnimationManager::ExecuteAnimation`
  (fixed in review: `AnimationRefreshStub` is driven by the trap's healed
  query plus a one-shot per-group marker - not the live DB byte, which the
  heal itself flips to fetched). The per-asset stubbed mark lives in the
  entry-table state (vfs), not in the direction structs.
- Alternative designs rejected: a stub-event callback/registry
  (file→CIndexObject mapping is game knowledge and would need reverse maps
  and locks), and generation-counter polling (same mapping problem, more
  state). Index-build marking + lazy trap-driven refresh needs neither.
- Object lists (`m_UsedGumpList` etc.) may hold the refreshed object twice
  (destroy clears `UserData` but does not erase; the reload pushes again).
  Harmless: every list consumer null-checks `UserData` before acting.

## Fixed in review

Three blockers found in review, closed in place:

- **C1 - healed UOP gumps re-decoded with stale 1x1 dims** (heap overflow).
  The gump stub wrote `W=1/H=1` into its payload and `UopReadIndexFile` bakes
  those into `CIndexObject::Width/Height` at boot; on heal only
  `UserData`/`Stubbed` were cleared, so `GetGumpPixels` (no bounds check)
  wrote thousands of runs into a 1x1 pixel vector. Fix, both ends: the stub
  header now writes `W=0/H=0` (readers short-circuit on `blocksize == 0`,
  `UopReadIndexFile` bakes 0x0, `Address` still points at the stub), and
  `vfs_refresh_index` re-reads the real dims from the healed payload when the
  refreshed entry is UOP-gump (`io->UopBlock != nullptr` and the containing
  mapping's stub kind is `vfs_stub_gump`, via `vfs_trap_stub_kind_at`). The
  dims live at the block start, i.e. `Address - 8`: `UopReadIndexFile` reads
  them at `Start + Offset + MetadataSize` and only then does
  `Address += 8; DataSize -= 8`, and a heal never moves the block (same plan
  offset/`ml`), so the re-read mirrors the bake exactly.
- **Dead refresh hook - the animation hot-swap hook was dead code.**
  `AnimationRefreshStub` required the state DB byte to still be `stubbed`
  **and** the trap asset to be healed, but the heal itself moves the DB byte
  to `fetched` before the trap marks the asset present - the intersection was
  empty and cached stub frames were never dropped. Fix: the hook is driven by
  the trap's healed query (`vfs_refresh_block`) plus a one-shot marker per
  animation group (`s_anim_refreshed_groups`), dropping the group whole (all
  directions share the group's block) exactly once.
- **Stub promotion - reconcile permanently promoted stubs to fetched.**
  `trap_stub_patch_entry` writes the plan's stored shape (`Flags=0`/
  `CompressedSize=ul`), so a stub's patched entry is byte-identical to a
  materialized entry and a shape-only `vfs_store_pack_asset_present` reported
  it present - reconcile upgraded stubbed (and pending, after a heal crash)
  to fetched on that alone, making the placeholder bytes permanent. Fix:
  reconcile never promotes `stubbed` from on-disk presence at all (only the
  heal path - the materialize write plus its journal `F` - promotes), and
  pending promotion requires the reconcile-grade presence check
  (`blobs_dir` given): the stored payload head must match the blob's
  decompressed head (missing blob = not present). The pure shape check stays
  the trap's boot-time visibility rule - a stub patched by a previous session
  must stay visible to `UopLoadFile`.

## Known limitations

- **Mid-session stubs of assets absent at boot stay invisible until the next
  boot** (MINOR 1, both UOP and loose): the index tables are parsed once at
  boot, so an asset that was still absent then keeps `Address == 0` (pack
  entry never patched, loose idx row never read) and the hot-swap refresh -
  keyed on a valid `Address` - cannot fire for it. Everything stubbed
  in-session (or healed in-session) hot-swaps normally.
- **Uninitialized scratch read for absurdly small anim blocks** (MINOR 2,
  pre-existing, narrowed): `DecompressBlock` memcpy's `min(cl, ul)` bytes
  into a caller buffer sized `ul` (xuocore/uodata.cpp DecompressBlock), and
  `UopReadAnimationFrameInfo` then reads the fixed-offset
  `AnimationFrameInfo` at `[552, 560)` - a tiny real block would leave those
  bytes unwritten. The scratch buffer there is zero-initialized now (1-line
  hardening), so the read yields zeros instead of heap garbage; not
  refactored further in this pass.
- **Loose-file caches** (texmaps/light on legacy installs): index marking
  works through `MulReadIndexFile`, but a *mid-session* stub of a loose file
  whose sprite was already cached is only refreshed by the garbage collector
  (10s), not the hot-swap hook. Rare (T1 files, typically complete before
  play); deferred.
- **Speech/Cliloc/MultiMap** have no refresh hook: their tables load at boot
  or per-use (`CMultiMap::LoadMap` re-reads every open), so they self-heal
  without extra code. `ClilocManager` has no MainMisc read at all (only T0
  `Cliloc.<lang>` files); the MainMisc strings reader is `SpeechManager`.
- **Verify**: `xuoi --verify` reports a stubbed pack block as corrupt (entry
  flags/sizes differ from the plan by design). The state DB is the stub
  truth; verify/seal handle `vfs_stubbed` (see "Verify/seal stub semantics"
  below).
- **Gump/art overlap**: `artLegacyMUL.uop` serves land tiles with the same
  hash-space as statics; the run-header stub leaks two nonzero u16s into the
  raw-land pixel read (2 of 1012) - visually a couple of near-black specks
  on an otherwise transparent placeholder tile.

## Review residuals (latent, non-blocking)

- Persisted ct=0 stub patches read as boot-present via pure-shape checks
  (`vfs_trap.cpp` boot visibility): such a gump stays empty until remap even
  though the DB honestly says `stubbed`. Pre-existing trap behavior; heal
  paths and reconcile are honest since the reconcile fix.
- `s_anim_refreshed_groups` one-shot markers never clear: a group re-stubbed
  within the same process (requires remap) will not re-fire its hook; stale
  sprite survives until `ClearUnusedAnimations` eviction. One-shot rationale
  documented at the hook.
- ct=0 presence head-check (64B) could theoretically match a zero-headed real
  payload against a misc zero-stub; vanishingly rare, content CRC would close.
- `vfs_stub_kind_for` matches any pack name containing "gump" while the gump
  dim handling in `UopReadIndexFile` is exact-match `gumpartlegacymul`; exact
  pairing holds for all current packs — a future "gump*" pack must keep the
  `+=8` dim convention or both sites must be updated together.

# NOTES — deferred decisions and known limitations (M5)

## Non-sparse filesystems: warn, no append-layout

The designed append-layout fallback for filesystems without
sparse files (fat/exFAT) — blocks appended as downloaded with per-entry
`Offset` patches — **is deliberately not implemented.** The trap's page
coverage model depends on the static planned offsets (the plan *is* the
coverage map; `vfs_trap_map` builds per-asset ranges from it at mapping time),
and an append layout moves blocks to file-end offsets that only exist after
download - supporting it means a second coverage model, remap-on-grow (POSIX)
or staged assembly (Windows), for a rare case.

What ships instead: after the store prepares a run,
`vfs_store_fs_supports_sparse` probes the filesystem once (a 1 MB hole, then
`st_blocks` on POSIX / `FILE_ATTRIBUTE_SPARSE_FILE` on Windows). On a
non-sparse volume the store logs a loud warning with the planned install size
("up to N MB will be preallocated") and continues; everything works, it just
really allocates. The probe is covered by a unit test on sparse filesystems;
the warning path itself needs an actual fat volume (manual verification only -
no such filesystem is available to CI).

## Update flow semantics and residual limitations

- An online boot downloads the product manifest fresh; a serial change
  rebuilds the plan over HTTP. Cached `*.mft` manifests are purged on drift
  before the rebuild (the store mirrors the URL tree, so stale manifests would
  otherwise shadow changed ones forever); part blobs stay (content-addressed).
- The old and new plans are diffed per asset (timestamp/rh/sizes/metadata -
  the same fields `xuoi_listing` diffs). Changed assets: state reset to
  pending (journal op `R`), pack entry `Offset` zeroed via
  `vfs_store_invalidate_asset` (the hole re-opens, the file loses its sealed
  status), cached blob deleted. They heal through the normal paths: tiers for
  T0/T1, the trap for T2/T3, `xuoi --full`/`vfs_fetch_all` headlessly.
- **Serial change with identical content** (a re-signed manifest that changed
  nothing else) logs "update: new serial, no differences" and touches
  nothing. **Same serial with changed content** under an existing manifest
  tail is still invisible - the limitation of the content-addressed store; it
  would need a per-tail timestamp index in the store (deferred, not planned).
- State indexes are plan-order positions. The diff/reset assume the new plan
  keeps the manifest order (the deterministic plan makes this true for
  re-signed manifests); a manifest that *reorders* entries makes old state
  bytes misalign (pre-existing behavior on serial drift: `vfs_state_load`
  rejects only size changes). Still open.
- Loose-file reconcile is now content-aware when the store root is known
  (`vfs_store_loose_asset_content_ok`: head compare against the blob,
  inflating the head for zlib assets). Before that check existed, a boot
  skeleton (right-sized
  sparse file) promoted to fetched at the reconcile inside the first
  materialize pass, which permanently shadowed its blob - a loose T2/T3 file
  could then never converge (surfaced by the full-install convergence
  work). Without a blobs_dir the old size-only check still applies.

## Verify/seal stub semantics

`vfs_verify`/`vfs_store_verify` take the state DB: an asset marked
`vfs_stubbed` reports `vfs_verify_missing_block` (bucket) with the asset's
`stubbed` flag set (vuoi prints `STUB` as the label), never `corrupt` - its
on-disk entry is a placeholder patch. Sealing is state-driven and
unchanged: stubbed (like pending/failed) blocks the seal mark, and the file
re-seals through the normal materialize write once the real block lands.

## `--streaming-trap=off` (escape hatch)

`vfs_params::traps = false` maps managed files without any page protection:
pointer reads of undownloaded ranges return the sparse file's zeros, no fault
handler is installed (the process keeps its default SIGSEGV/VEH disposition),
and the fault counters stay silent. `vfs_ensure_range` (the GetData/GetRaw
pre-resolve) still fetch + materialize on demand;
stub payloads are never served (their pages are readable, so nothing faults
into the stub path).
mappings made before the flag flips keep their protection. The HUD shows no
placeholder counts in this mode (nothing is ever marked stubbed).

## HUD

`CVfsStatusHud::Draw` (src/ScreenStages/VfsStatusHud.cpp) is called once per frame
at the end of each screen's `Render` - `CBaseScreen::Render` (main menu,
server/char list, creation, prof/town pickers) and `CGameScreen::Render`
(in-game) - right before the frame is presented; main thread only. (Drawing
after the screen render would paint into the void: every screen presents its
own frame at the end of `Render`.) It polls
`vfs_hud_stats` (an in-memory snapshot over the boot-owned state; no disk io,
no reconcile - the full `vfs_stats` maps every pack and would hitch the
render) a few times a second, shows activity (rate + done/total), a
"placeholders: N" warning while stubbed assets are in use, and an offline
badge; it fades out ~2 s after the last activity and costs one flag check
when streaming is disabled. It is deliberately not a gump and not registered
in the gump manager (passive, non-interactive, visible on every screen).

## Still deferred

- MUL idx-range streaming: unchanged - loose files resolve whole-file
  on first fault; the idx-range covering map + range-GET coalescing remain
  future work (see the whole-file note above).
- Retry scheduler: the next-boot retry stands; no in-session retry
  rework has landed.
- The original "zero requests" test now asserts *one* request on a full-cache
  online boot (the product refresh); everything else must stay request-free.

# NOTES — deferred decisions and known limitations (M6)

## Web groundwork: what exists now, what a browser port must still build

The groundwork lands the VFS-side seams the Web backend plugs into,
minus everything browser-side: the `vfs_transport` pluggable transfer seam,
the
granule-resident `store_arena` backend (heap arenas holding decompressed
payload bytes) with LRU eviction, the
product-subset filters and the fixture server's COOP/COEP headers.
All of it is verified on native; nothing includes Emscripten code.
The default configuration (sparse backend, curl transport, no subset) is
byte-identical to the pre-groundwork behavior, and the whole native test
battery runs unmodified on it.

What a browser port must still build (beyond the platform plumbing - WebSocket
game network, MEMFS/IDBFS layers, SDL2/audio, the requestAnimationFrame main
loop, which are all outside the VFS):

- The transport implementation: a `vfs_transport` subclass over
  `emscripten_fetch`/JS-`fetch` (wired through `vfs_transport_set`), with
  `range()` implementing granule range reads (the curl default reports
  range unsupported and keeps the whole-fetch fallback). Decompression of
  part blobs can use `DecompressionStream("deflate")` in a worker.
- Persistence: Cache-API/Service-Worker or OPFS granule store behind
  ensure/evict; the state DB (plan.bin/state.bin/journal semantics) moves to
  IndexedDB as the advisory bitmap.
- The async boot: `vfs_boot` is synchronous on native. On Web the T0
  "blocking" tier must become an await (or a loading screen over repeated
  `vfs_is_ready` checks); the seam (the fetch engine + the arena) already
  answers every miss asynchronously (fetch engine + arena ensure).
- Index baking: `CIndexObject.Address` must point at arena slots
  (via `vfs_store_arena::read`), `0` when not resident, plus the
  refresh hooks at every consumer - mandatory on Web, where no blocking
  fault path exists.
- `vfs_store_arena::max_bytes` must be sized to the wasm32 headroom
  (default 256 MB) and `#if defined(__EMSCRIPTEN__)` code goes behind the
  transport/`cancel` seam and the persistence wrapper - not into the engine.

Web-port backlog (review findings, port separately):

- the product-manifest refresh (`http_get_binary`, vfs.cpp:~1032) predates
  the transport seam and still uses plain curl;
- the `.exe`/`.dll`-ish exclusions are not implemented (the pattern
  table covers anim\*.mul/Music/facet\*/verdata\* only);
- the exact body cap converts an oversized-body mismatch into (bounded)
  retry churn instead of an accepted transfer - manifests must size true.

## Arena eviction and consumer liveness (THE arena rule)

`evict()` and the post-ensure budget enforcement drop LRU granules; a
granule with an in-flight `ensure()` is never evicted (its commit would
resurrect bytes the budget asked to drop, and the waiter would read freed
memory). Everything else - sealed granules included - is evictable, by
decision: arena bytes are always recoverable from the blob store, and arena
mode pairs with the no-traps configuration where a later `ensure()`
re-materializes them. The consequence is the arena consumer-liveness rule:
no cached game object may keep a pointer into arena bytes
across a possible eviction. The native answer is the refresh machinery
(`vfs_refresh_index`/`vfs_refresh_block`: destroy the cached sprite, clear
the flag, reload on the next frame); the Web port wires the same
`DestroySprite` path. Arena residency deliberately holds payload bytes only
(pack header/entry tables are not resident: the Web UOP table synthesis is
in-memory), so `read()` bounds are payload bounds, not file-image
offsets.

## Subset mechanics

`vfs_manifest_apply_subset(plan, subset)` filters the plan at every (re)build
(`vfs_subset_web` drops `anim*.mul`, `Music/**`, `facet*`, `verdata*`; see
`vfs_subset_web_excludes`), and `plan.bin` (serialization version 3) records
the subset as one trailing byte - a v2 payload is exactly a v3 payload
minus that byte, so v2 plans load unchanged with subset none (an older client
reading a v3 plan.bin rejects it and rebuilds: the drift path, harmless).
Removed-by-subset entries behave like plan-removed in the update flow: the
store keeps their bytes, the accounting drops them. A stored plan.bin whose
subset byte differs from the params subset is drift in BOTH directions
(web→none and none→web): the plan rebuilds with the params view and the
diff/reset runs like any serial change. That rebuild changes the plan's
asset count, and state.bin is plan-ordered - the cleared DB now also
truncates journal.log, flushing the fresh all-pending state.bin BEFORE the
truncate (the journal rotation safety rule), so stale fetched marks cannot
resurrect over the reset; a subset switch therefore always re-verifies but
never corrupts. The sparse install never prepares excluded
files, so a boot that ran without the subset keeps stale skeleton files
that verify as missing blobs (delete the install or run `xuoi --verify`).

## Transport seam details

The transfer mechanics moved from vfs_fetch into the curl implementation of
`vfs_transport`; the engine keeps queue, dedup, retry policy and the
buffered-bytes backpressure. Two deliberate deviations from the pre-seam wire
behavior, both invisible to the tests: the per-transfer body cap is now the
exact expected size (the earlier 64 KB slack was a grace window for
oversized bodies that the engine rejected anyway - oversized bodies now
abort mid-transfer instead of after it), and the manifest seam's
per-call curl handle became a per-thread one (keep-alive improves; the
wire protocol does not). The vtable carries the absolute URL (and not
tail + repo) because sub-manifest URLs are composed by the mft engine from
the product manifest's own `<manifestrepos>` - on a first boot those repos
are not known to the fetch engine, so the seam must take resolved URLs.
Injection is process-wide and thread-safe (`vfs_transport_set`); tests own
their fakes and restore the previous override afterwards.

## Arena boot on native (memory-mode)

`--streaming-backend=arena` (or `params.store_backend`) boots without
materializing any install file: the skeleton prepare, the managed-mapping
registry and the trap backend are all skipped (no traps exist on the arena
backend), T0 granules are ensured resident through the
backend, and the
background tier ensures everything reachable from the store. The state DB
keeps its meaning (fetched = resident this session, stubbed =
placeholder, holes pre-marked), the HUD counters read it unchanged, and
`vfs_stats` skips the on-disk reconcile (there is nothing to reconcile
against). Residency contract: boot re-ensures every T0 granule (blocking -
core assets are small) at EVERY start, restarts included, so the core bytes
are resident again after a restart; T1+ granules are converged by the first
boot's background pass and afterwards repopulate only on access (the
consumer `ensure()` path - the native memory-mode wires no
game layer, so nothing else touches them in between). state.bin stays the
advisory record: a warm re-ensure that fails demotes the asset to pending
(journal `R`), so the next-boot retry semantics stand. What the native memory-mode
deliberately does NOT do:
serve the game (no index baking, no mappings - files do not exist), verify
(`vfs_verify` inspects files) or materialize for the CLI tools; those stay
sparse-backed on native and belong to the browser port's runtime.

## Transport + arena test surface

`tests/vfs/test_arena.cpp` runs the store_sparse core battery re-mixed for
arena semantics (ensure/is_ready/read round-trip over zlib, stored, hole
and loose assets, prefetch + demand heal against the fixture server, LRU
eviction order with budget edges 0/oversized plus the read-touch
LRU-vs-FIFO discriminator and the in-flight eviction probe, stub fill +
heal, seal, the warm-boot T0 residency contract), the
subset unit tests (pattern table, idempotency, plan.bin round-trip, v2
compat, drift rebuild re-applying the subset, the persisted-subset policy
rebuilding in both directions) and the transport seam tests
(a fake transport serves the full boot - manifests and blobs, no curl no
sockets - plus the range-unsupported contract, the in-flight eviction
probe and the COOP/COEP fixture
server headers). The original "zero requests" assertion is untouched and still
passes on the curl transport.

## Current legacyrelease gumpart format (flags == 3, zlib + bwt)

The product's `gumpartLegacyMUL.uop` (distributed whole, uncompressed, via
`unpacked.mft`) stores per-gump payloads as zlib streams that inflate to a
bwt/mtf coded stream (MYP Flags == 3); only after both steps does the payload
read as `[u32 width][u32 height][row offsets][RLE]`. Supported since the
ClassicUO BwtDecompress port: `DecompressBlock`/`GetData` decode Flags == 3
blocks, and the gump read path decodes compressed entries on first use,
caching their dimensions (verified: statusbar gump 0x0804 decodes to 154x60
from the current file; regression test `uop_gump_dims_validation` covers the
dimension guard).

Known open edges:
- [resolved] assembled packs (art, maps) materialize stored payloads
  (Flags == 0, layout spacing by `ml + ul`): the materializer inflates each
  zlib blob right before the block write, so raw-pointer readers (art, map
  blocks) see stored bytes like EA's own local archives. Plan/state
  serialization bumped to v4 - persisted plans rebuild and states reset on
  the next boot. An entry-table page that also carries an absent block's
  payload is re-guarded after a stub entry patch (the patch used to leave it
  readable, silently disabling demand fetch for that block).
- cliloc files and the string dictionary carry an 0x8E byte at offset 3 when
  bwt coded; both decode up front now.

# NOTES — streaming update verbs and the update wave (M7)

## What the update surface is now

- The update *engine* was already M5's drift flow (prod refresh every online
  boot, serial drift, manifest purge, plan rebuild, per-asset diff, reset).
  M7 adds the surfaces around it; no engine semantics changed.
- `vfs_params::update_check` (default true). false =
  `--streaming-no-update`: `vfs_boot_product_fetch` keeps the cached product
  manifest (a MISSING one still downloads - a first boot must plan). The
  persisted plan then loads undrifted and streams on unchanged; missing
  assets still fetch over the network (this is NOT the offline mode).
- `vfs_update_check(vfs_update_info*)`: the read-only "is an update
  available" verb. Downloads the product manifest into MEMORY (the cached
  copy and the plan stay untouched - the next boot applies what this only
  reports), compares serials, and treats a subset change as drift too (a
  subset change rebuilds+diffs at boot even on the same serial). A missing
  plan reports `update_available` (nothing installed is actionable). It
  calls `http_init` itself (idempotent) - it runs without a boot; call it on
  the boot thread only (the http layer's handle is a process global).
- Update wave: assets the plan diff changed in the ON-DEMAND tiers (T2/T3)
  are appended to the END of the T1 background list (`vfs_boot_wave_append`,
  after the recorded-order sort). An update therefore converges T0 blocking
  before the first frame, T1 behind the game, T2/T3 in the same background
  thread - no holes left for the first access to trip over, no demand and
  no `vfs_fetch_all` needed. The exact-once refetch in
  `update_serial_bump_refetches_only_changed` now proves the diff deleted
  the blob (a present blob is never re-requested); that test's old
  "changed asset stays a hole after boot" expectation became false BY
  DESIGN with the wave and was rewritten.
- Update still ACTIVATES at boot (index pointers are baked absolute; a
  mid-session activation would need remap + index rebuild). What the wave
  buys is that the download is already done when that boot happens.

## CLI verbs (xuoi + vuoi, same shape)

- `--check`: read-only serial compare; exit 0 up-to-date, 1 update
  available (or nothing installed), <0 the check failed.
- `--update`: boot (applies the diff) + `vfs_fetch_all` (the delta is
  exactly "every missing blob" after the reset) + materialize + summary;
  no verify (`xuoi --full` remains the sealing pass).
- `vuoi` params builder now defaults `blobs_dir` to `<install>/cache/blobs`
  (the client's layout) so the verbs work without `--blobs`.
- Launcher integration is deliberately OUT: the launcher's update
  functionality is being removed; game-data updates live in the CLIs and
  the client's launch-time flow.

## Deferred (explicit product direction)

- T2/T3 must become a CONTINUOUS background download that always prioritizes
  on-demand (render-critical) requests over the sweep. The update wave is
  the interim step (changed assets only, riding the T1 thread); the full
  background sweep is later work.

# NOTES — the continuous background sweep and the in-game demotion (M8)

## What replaced the on-demand-only contract

- After the T1 tier (and the update wave) drain and the store->install
  convergence finishes, the background thread now keeps going: the
  continuous sweep downloads every remaining on-demand asset (T2 first,
  then T3, plan order within a tier) until the install is complete.
  `vfs_boot_wait` includes it (headless verbs like `vuoi --streaming-fetch`
  now converge the WHOLE product - that is what "fetch what is missing"
  always promised); the game never waits for it (the sweep starts only
  behind the first-frame gate and runs for the rest of the session).
- The sweep list is collected at boot (states still exclusive to the boot
  thread) and re-checked per chunk at run time: anything the game
  demand-healed (or stubbed) while it sat queued is skipped. Stubbed assets
  keep their on-demand heal path entirely - the sweep never touches them.
  Holes stay holes until access. Range assets stream their own blocks on
  the background thread exactly like T1 chunks do.
- `vfs_params::background_sweep` (default on; `--streaming-no-sweep` on the
  client and vuoi) restores the old on-demand-only contract - the escape
  hatch for constrained setups and for tests that exercise demand paths.

## Priorities and the in-game demotion

- Priority ladder, ascending precedence: sweep 3 (`VFS_FETCH_PRIO_SWEEP`)
  < arena demand granules 2 (unchanged) < background tier 1 (T1, update
  wave, `vfs_fetch_all`) < demand 0 (traps, T0, `vfs_fetch_blob_to_store`).
  One latent inversion was fixed on the way: background whole-blob
  prefetches used to ride the DEMAND lane (0) because the priority was not
  plumbable - `vfs_fetch_and_materialize_asset` and
  `vfs_fetch_blob_to_store` now take a priority, and the background thread
  passes its own.
- The sweep lane: at most `active_workers - 1` sweep-priority jobs run
  concurrently (clamped at 1 on one-worker pools), so a worker always
  stays free to pop demand instantly. `vfs_fetch_pop` skips sweep jobs
  while the lane is full; the worker wait predicate understands runnability
  (parked/budget/lane) so nothing spins.
- In-game demotion (`vfs_fetch_set_ingame`, called from
  `vfs_notify_first_frame`, one-way): the pool parks one worker
  (`s_active_target` N -> N-1, clamped at 1). The default pool is already
  cores-1 (cap 8), so with defaults the download pool runs cores-2 in-game
  - one more core kept clear than at boot - and the sweep lane drops to
  active-1 on top. Parked workers sleep between jobs (the wait predicate
  owns the decision), in-flight jobs complete, and `vfs_fetch_stop` wakes
  everyone as before. Headless callers never demote.
- Test server gained a `/slow/` path segment (stalls 0.3 s, then serves
  the remainder) for deterministic in-flight ordering (the sweep-lane test).

## Shutdown mid-streaming (vfs_shutdown must signal the tier)

- `vfs_shutdown` never set `s_boot_shutdown` - only `vfs_boot` reset it.
  Quitting mid-stream therefore killed the fetch pool and then joined a
  background thread that kept walking its chunk loops against the dead
  pool: every chunk logged a materialize pass plus "fetch: pool not
  running" until the whole sweep drained (minutes on a real install) -
  and quitting before the first frame hung FOREVER on the tier's frame
  gate. Both fixed by setting the flag (already atomic) before
  `vfs_fetch_stop`; loops exit at chunk boundaries and the frame gate
  polls the flag. Regression tests: shutdown before the first frame
  (mutation-checked: hangs without the fix) and shutdown mid-sweep with
  a reboot-after check (the journal replays what landed).

## Concurrent demand on an object the sweep is already fetching

- QUEUED (lane full / workers busy, not started): the dedup in
  `vfs_fetch_enqueue` PROMOTES the queued job to the new request's
  priority - a demand landing on a sweep-queued tail jumps the queue
  (`fetch_demand_promotes_queued_sweep` proves it across the stalled-lane
  window; promotion updates BOTH job copies, the queue node pop reads and
  the shared job the completing worker reads, and only while still queued).
- IN FLIGHT (worker mid-transfer): the demand dedups onto the running job
  and waits - no second GET. A wire transfer is not preempted; the wait is
  bounded by the blob itself, and the native trap's 30 s timeout degrades
  the fault to the stub path if it overruns (the sweep's chunk materialize
  heals that stub right after the blob lands).
- RANGE assets (native pipeline): both the sweep chunk and the trap fault
  issue direct transport range GETs on their own threads - the engine
  queue is not involved, so there is no dedup and no preemption; the game
  thread downloads its block in parallel. Two GETs of the same block can
  therefore overlap in a one-block window; both materialize under the
  state mutex and the second write is idempotent (identical bytes). Known
  minor inefficiency, no correctness impact - in-flight range dedup is
  future work if it ever shows in the bandwidth profile.

# NOTES — the aops patch format (M9)

## What aops is and what the plan build does with it

- Third-party patch-manifest format ("AOS-patcher" family, uniteuo-hosted
  shards). A patchcfg INI ([project] custom_extension/patch_salt_url +
  [patch_system_configuration] online_patch_directory/online_patchlist_file/
  update_local_file) beside a UTF-16LE pipe table of EVERY file the shard
  ships: <relpath>|<DDMMYYYY>|<hh>|<mm>|<ss>|0|<size>|<flag>|<flag>|<hash>.
- The hash does NOT verify the served bytes (stale patcher-side value;
  verified against live files) - the file SIZE is the check; the salt
  belongs to the launcher's own archives. Both are carried informationally.
- vfs_source_aops.cpp: sniffers (patchlist/patchcfg over product bytes),
  the ini parser (relative urls resolve against the pointed root), the
  UTF-16 decoder, the UO data filter and the plan builder. The filter is a
  compiled allowlist derived from a stock client tree: whole classes
  (.mul/.idx/.uop/.def/.bin/.iff), the client's language families
  (cliloc/skilname/skill16/skill30/chat/gesture/intloc/intro/options/
  opt*/professn/tilehelp/tooltips/gt_ + the 8 languages) and an exact-name
  table (doors.txt, tile256.txt, obscene.lst, ...). Client config
  (uo.cfg, login.cfg, ...) and launcher/web files stay out: the live fc
  server plans 338 data files out of 2072 entries.
- Names install LOWERCASE (the managed registry already normalizes both
  sides). Blob tails are the url-encoded relpath + the custom extension
  (test1/tiledata.mulaops") - the server concatenates name and
  extension with no separator. Wire stamp folds into YYYYMMDDhhmmss so
  plan diffs see date bumps.
- The aops product IS the patchlist: product_url points at (or derives
  from) the root's patchlist; the serial is a lookup3 fingerprint of the
  patchlist bytes (vfs_product_serial sniffs: MythicMFT XML -> serial
  attribute, otherwise content hash). vfs_update_check, drift, diff,
  journal and the whole tier/sweep machinery work unchanged.
- The uniteuo servers serve HTTP Range, so after the build the whole-file
  .uop entries refine into per-block range assets exactly like the official
  product (94k range blocks on the live fc server) - page-fault streaming
  applies to packs too, and nothing ships "unpacked".
- Pointing at a bare server root works: a product fetch that returns
  neither format retries once at <root>/patchlist. The game grew
  --streaming-product (url or file; aops roots normalize via
  vfs_aops_product_url, dotted manifest leaves pass through untouched).
- Tested: offline filter/normalize/dedup plan build; full http pipeline
  (patchcfg + patchlist fetch, T0 blocking install, fetch_all converge);
  update check; serial-drift re-fetching exactly the changed file. Also
  validated against the live uniteuo fc server end to end.

---

# XUO Assist — assistant internals & decisions

Topic notes for the in-client assistant (`src/Assistant/`): durable
gotchas, accepted deltas and the reasoning behind them.

## Build gates & layout

- Renderer gating: the assistant builds only for GL3/GLES2 (it renders
  through the vendored ImGui GL3/GLES2 stack). GL1/GL2 builds exclude it
  (`XUO_ASSISTANT_ENABLED` in the root `CMakeLists.txt`).
- A PRIVATE compile definition on the `crossuo` target does not reach the
  `xuoassist` static lib's own sources — the lib carries its own
  `target_compile_definitions(xuoassist PRIVATE XUO_ASSISTANT=1)` (a missing
  define went unnoticed at first because Linux allows undefined symbols in
  shared libs).
- With `XUO_ASSISTANT` on (non-Windows), the client builds as ONE
  self-contained `crossuo` executable with a real `main()`. `XUO_ASSISTANT=OFF`
  builds the same single executable without assistant symbols or strings; the
  old `client` dlopen loader + `libcrossuo.so` plugin pair is gone.
- Link guard: `crossuo` links with `--no-undefined`. The assistant is part
  of the shared lib, and a declaration that never got a definition used to
  slip through as an undefined symbol that aborted the client on the first
  assistant frame.
- Cutover: the assistant is the only consumer of the client's packet taps;
  the 0xFC list feed (skill/spell/macro lists) is sent at startup via
  `CGame::SendAssistantLists()`.

## Lifecycle & visibility

- Init may run from `AssistantWindow_DrawOverlay()` or
  `AssistantWindow_Toggle()` (Alt+U); both run on the main thread with the
  game GL context current at the in-game stage. On init failure the hotkey
  falls through to the game and an error is logged once.
- The assistant exists only in-game: logout, character select or disconnect
  shut it down, whether its windows are docked or popped out.
- No ImGui NewFrame runs while hidden, so residual pop-out platform windows
  freeze until the assistant is re-shown. Accepted.
- The assistant draws after `RenderDraw_Execute`: it paints over the game's
  software cursor when overlapping; ImGui's SDL backend shows the OS cursor
  over assistant windows. Cosmetic.
- Pop-out behavior (drag-out, dock-back auto-merge, popped-out windows
  while the game window is minimized) was compile/inspection verified; a
  manual interactive pass is still the real check.

## Rendering & texture lifecycle

- Texture shutdown ordering: free CPU pixels and delete textures only AFTER
  the ImGui context is gone (`AssistantUI_AfterImGuiShutdown`), because the
  backend reclaims GL names inside `ui_shutdown` by walking registered
  textures with `RefCount == 1`. Applies to the icon atlas, AssMap and the
  About logo alike.
- Textures stay registered with `RefCount == 1` so the backend's teardown
  walk can find them; runtime-evicted preview textures use the same rule.

## Command queue & script runtime

- Only script worker threads push; only the main thread drains
  (`ScriptQueue_Drain`), called from `AssistantUI_Tick` before the agent
  ticks and before the frame draw — inside the game's frame loop, so the
  drained commands see the same world state a main-loop consumer would.
- Every command carries a promise side (mutex + cv + flag); the pushing
  script blocks on it. The queue is shut down before the script threads
  join so late pushes fail fast instead of deadlocking.
- Termination: a JS interrupt handler polls the terminate flag inside every
  JS loop (between-API-call pauses never reach a tight loop otherwise).
  The Wait/Pause flag family clears + polls its received-flag inside the
  drain, so the main-thread flags are never touched cross-thread.

## Input & hotkeys

- The client never calls `SDL_StartTextInput` (only the vendored ImGui SDL2
  backend, Android-only), so `sync_text_input` toggles SDL text input while
  `io.WantTextInput` and restores the prior state on hide/shutdown. Known
  one-frame lag on the first keystroke into an ImGui InputText.
- While hidden, the hotkey engine keeps working (the assistant could be
  hidden and its hotkeys keep firing; Pass-keys and the On/Off switch
  govern it). The capture field ("listening") must see raw events even
  when the game would; it is gated on `!WantTextInput` so a busy text
  field always wins.
- Key auto-repeat never fires a binding: the game only sends non-repeat
  key events to the engine.

## Persistence

- All saves are crash-safe (`AtomicFileWrite`, `config/xml.{h,cpp}`): temp
  file in the target directory → write → `fflush` → `fsync`/`_commit` →
  preserve the original file mode → atomic `rename`/`MoveFileEx`. All
  `XMLDocument::Save` sites (incl. `Map.xml`), `SaveDress` (Dress.coa), the
  script text window save and the script editor's Save funnel through it.
  Any failure keeps the original file; stale `*.tmp` files are ignored on
  load and truncated on the next save.
- Format quirks kept for byte compatibility with existing profile files:
  `FindList.xml`/`IgnoreList.xml`/`Hotkeys.xml` store `size = count+1`;
  Map.xml spells one attribute `other_sho_name`; Dress.coa strings carry
  the length+1/0x00 layout and the reader tolerates a full int32 length
  (names failing the bounds check come back empty, the set is skipped).
- Multi-instance: two clients sharing one game directory share `Config/`;
  files are not locked, so simultaneous writes to the same profile can lose
  updates — use separate profiles per client.

## Packet taps & macro recording

- Every macro-recording tap in `packetmanager.cpp` is observe-only: a
  single `IsRecording()` check when idle, packet flow untouched. The taps
  build the same `CMacro` subclasses the actions map to (`MT_USE` →
  `MCT_SELF`; `MT_ATTACK` → `MCT_LASTATTACK`/`MCT_LASTTARGET`/`MCT_ENEMY`
  Note: after unifying the last-cast/skill indices with the client globals, the
  MCT_LAST conversion at the 0x12 sites is effectively always-on when recording
  (the client assigns the index before the record call runs). Playback is
  unaffected - the recorded entry is the cast itself.
  in that order; cast variants → `MCT_LAST` on the previous-index "was-set"
  comparison; `MT_COMBAT_ABILITY` → primary/secondary via `g_Ability[0]`;
  `MT_WALK` records the negative turn encoding on facing change).
- `PlayWaiting`: the gump, menu, context-menu, prompt and target managers
  satisfy a playing macro's wait on packet arrival and consume the packet
  (the macro replay answers the server).
- The shop agent taps the OpenContainer/BuyList/SellList replies in
  `packetmanager.cpp`; the progress countdown runs on frame ticks.
- Map window geometry: the legacy `Map.xml` window attributes are honored
  for first placement only; ongoing geometry lives in the ImGui window ini.

## Packet handler fusion — COMPLETE

The parallel assistant packetmanager (its own handler table, tapped before
the client's) has been dissolved into the client's handlers. Batches B1-B4
fused the recv side: every recv handler is now either fused into the client
handler, kept whole on the recv tap with a rationale, or deleted as a
duplicate of client state updates. B5 audited and closed the send side.
Table composition (`m_Packets` rows carrying handlers): 21 rows —
16 `SMSGH` send handlers, 4 `SMSGHB` rows (0x6C Target, 0x9A ASCII Prompt,
0xC2 Unicode Prompt, 0xBF Extended Command send half; each send half
records/intercepts the outgoing round and its recv half is fused into the
client handler) and 1 `BMSGH` row (0xFC, the last recv-capable row).
0x56 Map Data, 0x9E Vendor Sell List, 0x7C Open Menu and 0xB0 Open Gump are
fused into the
client MapData/SellList/OpenMenu/OpenGump handlers (mapecho gated on the negotiated map-echo
bit; the shop agent snapshot + auto-sell consume; the menu agent feed +
macro/hook consume; the gump agent feed + macro/hook consume). The remaining
recv-side seams are the
0xBF party send half and the 0xFC channel.
0x91 Second Login was demoted to a plain `SMSG` row; 0xD6 Mega Cliloc and
0xF0 Krrios client special are plain `BMSG` rows (both directions are
client-owned; the host fuses the MegaCliloc event).

Fusion checklist rule: a fused consume hook that parses the shared reader
must be Ptr-neutral (e.g. `CTarget::SetMultiData` calls `reader.ResetPtr()`)
or must restore the reader position before the client body runs — 0x6C
Target is the reference fix (`Ptr = Start + 1;` after the fused
`SetRecvData` parse so the client `SetData` reads from the payload start).

### Send tap (permanent seam)

The assistant's single send-side interception point is one call in
`CGame::Send` (src/CrossUO.cpp, pre-encryption, before the wire). It
dispatches through the assistant's own table: the `SMSGH` rows plus the
send halves of the `BMSGH` rows in
`src/Assistant/managers/packetmanager.cpp`. The client's send path is a
single function, so every producer (UI actions, macro playback, scripts)
funnels through it — there is no parallel client parse to drift from and no
second call site. The tap is observe/mutate-only: `AssistantPackets_Send`
returns void and the packet always reaches the wire; a handler's
`return false` (the "consumed" convention) has no send-side
effect. The table + tap is kept over collapsing the handlers into one
dispatch function because the `BMSGH` send halves already dispatch through
the same table — collapsing only the `SMSGH` rows would create two send
mechanisms, not one.

B5 send audit — each `SMSGH` handler is assistant-owned recording /
bookkeeping with no client send processing duplicated (the client sets its
own `g_Last*` mirrors inside its action functions; the assistant sets its
own `xuoassist::` mirrors from the wire, which also covers producers that
bypass the client action helpers):

- 0x02 Walk Request: auto-open-door probe + `MT_WALK` recording (observe).
- 0x05 Attack: `MT_ATTACK` recording with
  MCT_LASTATTACK/LASTTARGET/ENEMY conversions (observe).
- 0x06 Double Click: `MT_USE` recording + `g_LastUseObject` mirror
  (observe).
- 0x12 Perform Action: skill/spell/cast-from-book recording + last indexes
  + title redraw (observe).
- 0x72 War Mode: `MT_WAR_MODE` recording (observe).
- 0x7D Menu Choice: `g_MenuManager.OnPacketSend` (auto-menu agent
  bookkeeping).
- 0x9B Help Request: `MT_HELP_MENU_REQUEST` recording (observe).
- 0x9F Vendor Sell Reply: `g_ShopAgent.OnSellReplySent` (auto-sell agent;
  the recv-side 0x9E snapshot + auto-sell consume is fused into the client
  SellList handler, B10).
- 0x03 ASCII / 0xAD Unicode speech: fused into the client speech sites
  (CGame::SendASCIIText, GameConsole, macro speech) via the speech
  intercept - prefixed commands run and are no longer sent to the wire,
  MT_SAY records, own-speech font-color applies to the parsed color.
- 0x6C Target: interception + recording fused into CTarget::SendTarget.
- 0xB1 Gump Choice: `g_GumpManager.OnGumpChoiceSent` (auto-gump agent
  bookkeeping).
- 0x91 Second Login: deleted (see below).
All send handlers are migrated; the send tap and the assistant send
table no longer exist.

Target-hook interception (the from-target pipeline) is self-contained in
`CTarget` (core/target.{h,cpp}): arming `g_TargetHandler` opens the cursor,
the next outgoing 0x6C response feeds the armed handler, and
`RestoreTarget` re-dispatches the client's own saved request. The outgoing
0x6C is never dropped — the player's response still reaches the server; the
consume that keeps the client cursor-free lives in the fused client handler
(0x6C Target, via `SetRecvData`'s CheckHook/PlayWaiting consume).

Second Login (0x91): the assistant handler only reset `g_Connected`. 0x91
is sent exactly once per game-server session by `CGame::RelayServer` during
the login relay, before the game stage is entered, and
`AssistantPackets_CheckDisconnect` (top of `AssistantPackets_Send`) has
already cleared `g_Connected` whenever the stage is out of the game — the
reset was a no-op, so the handler was deleted and the send dispatch gate no
longer exempts 0x91. The row stays as a plain `SMSG` (the table-integrity
guard counts 256 named rows).

### Injection-guard policy

Assistant-injected client-bound packets (the `CPacket::SendClient` queue,
dispatched through the client table under the `AssistantInjectGuard` RAII
flag `m_AssistantInjecting`) run the same fused handlers as server packets.
This is safe by construction, and the invariant to preserve is:

- The injection set contains only assistant-published client-bound packets
  (system prints and text-replace reprints 0x1C/0xAE, light/weather/season
  sets, target requests/cursor re-dispatches, UI close/dye/reconnect
  packets). None of them may re-enter assistant filters or re-trigger
  state mirrors.
- The hooks that must not re-run on injected traffic are gated on
  `!m_AssistantInjecting`: the speech journal/filter/font-color re-entry
  gates, the weather/light/season filters, the morph patch (0x20/0x78),
  the no-gray-screen effect veto and the shop-agent OpenContainer consume.
- The state-mirroring hooks in EnterWorld/UpdatePlayer/UpdateObject etc.
  are ungated; that is safe because the injection set contains none of
  those ids. A sender adding a new injected id must check this invariant:
  if the id carries a fused state-mirror hook (or any filter/veto hook not
  gated above), the new injection must gate on `!m_AssistantInjecting`.

Recv keeps (consume-capable handler rows; the tap dispatches every packet to the assistant table):

- Injected: DisplayDeath 0xAF (last-corpse + auto-open hook sits before the
  owner-not-found early return, matching the old run-always tap); DisplayMap
  0x90/0xF5 (one client handler serves both ids; the map-echo corner
  assignments keep the 0xF5 exclusion); BuyList 0x74 (the shop agent's
  OnBuyList was already a no-op - the client stores price/name on the vendor
  items - so only the g_ShopReceived wait flag moved, set before the client's
  g_World gate); BuffDebuff 0xDF (the mode/timer/cliloc reads moved ahead of
  the client's buff-window guard, which has no side effects, so the buff
  mirror fires for own buffs whether or not the window gump exists; the
  script-visible name table became AssistantBuffName in assistantevents);
  Map Data 0x56 (mapecho translates the edit-response pin into absolute
  "map opened to" coordinates + tracker points - assistant map-engine state
  with no client-side producer; fused into the client MapData handler, the
  journal lines + tracker points fire only when the negotiated map-echo
  feature bit is set).
- Kept then fused: 0xBF left the
  keeps in B9: the party round was already fused and the remaining
  subcommands injected cleanly (see below); the SellList keep followed in
  B10 once the auto-sell consume moved in-handler, the Open Menu keep
  followed in B11 the same way, and the Open Gump keep followed in B12
  (the in-handler sub-reader feed made the consume expressible). No kept
  row remains: 0xFC is the only recv-capable assistant row. The
  connected-gate exemption list (0x1B/0x55/0xFC) is unaffected: every fused
  feed id arrives only with a world.

E2 (0xFC CrossUO messages): the twelve injected-packet builders were replaced by
extracted client methods (CGumpManager::CloseGenericGumps/SelectMenu/
ShowStatusbar/SecureTradeCheck/SecureTradeClose/MovePaperdoll,
CMacroManager::ExecuteFromLists, SendConsoleText, CGumpAbility::OnAbilityUse,
g_Game.StartReconnect/OpenDoor, direct statusbar close). HandleCrossMessages
keeps the six genuine-network OCT cases (features, ignore-tiles filter, version,
cast/skill requests, rename-mount) as the documented external subcommand surface.

E3 (client-bound prints): assistant system/character prints and text-replace
reprints call extracted client render cores (DisplayTalkTextA/W) directly.
Two builder-era artifacts are gone: reprints no longer rename unnamed objects
to the assistant name, and assistant prints can no longer hit the connection
screen during connect (the injected packets were processed by the wire
handlers' connect branch).

B9 (0xBF ExtendedCommand recv half) fuses the last recv-capable non-keep
row, subcommand by subcommand inside the client's switch:

- Case 8 facet map: the assistant half only wrote its own `g_CurrentMap`
  copy (clamped >5 → 0) for the map window's "Current" display; the client's
  `ChangeMap` applies the same clamp and drives the world, so the injection
  is the one-line sync `xuoassist::g_CurrentMap = ::g_CurrentMap`.
- Case 0x10 equipment info: the journal/inspector record rides the client
  handler's own gates (item found, main cliloc set): the inspector row
  refresh (`OnInspectorUpdate`), then a JMT_SYSTEM journal record at the
  0x3B2 color for the main line and a second one for the crafter/
  "[Unidentified"/attribute tail. The record re-reads the payload fields
  because the journal text substitutes the per-entry cliloc arguments
  (`ParseArgumentsToClilocString`) that the client render discards; the
  reader position is saved/restored so the client parse still starts at the
  serial field.
- Case 0x14 context menu: `g_ContextMenuReceived` is set, then the manager
  feed runs pre-parse (0x99 shape). Consume semantics preserved: a playing
  macro waiting on the menu (`PlayWaiting`) or the auto-select entry hook
  returning false vetoes `CGumpPopupMenu::Parse` (the client keeps no popup
  gump - the only downstream work the switch case owns); the feed-only path
  restores the reader before the popup parse re-reads the fields (the hook
  walk can consume entry bytes without a verdict). No `m_AssistantInjecting`
  gate: no `CPacket::SendClient` builder produces 0xBF with the fused
  subcommands (8/0x10/0x14); the only injected subcommand is 0x16 (close
  paperdoll), which carries no assistant logic - there is no replay path.
- Case 0x18 map patches: the assistant half was a no-op (the client applies
  the patches itself) - nothing to inject.
  Row demoted to `SMSGHB` (DIR_BOTH, send-only): the send half keeps the
  wrestling/cast/context-menu recording taps, and a `DIR_SEND` row would
  warn per packet because the server sends 0xBF constantly (party chat).
  The now-unused `parse_cliloc_arguments` helper went with the handler.

B10 (0x9E Vendor Sell List, the last recv-side shop keep) fuses into the
client SellList handler: the client parses the entries once (serial, vendor
lookup, per-entry graphic/color/count/price/name with the client's `GetA`
cliloc resolution - the snapshot names now carry the same text the sell
gump shows, where the old tap used `ParseArgumentsToClilocString` with
empty args) and feeds the shop agent from the parsed values (inline
`CVendorShopItem` mapping, the SecureTrading 0x6F style); the reader-based
`OnSellList(CDataReader&)` parse is gone, with the `cliloc_name` helper.
The vendor snapshot (serial + items) updates on every 0x9E regardless of
auto-sell, so the Shop List Editor keeps showing the vendor list while
auto-sell runs. The hook sits after the entry parse but before the
client's vendor/itemsCount early-outs - the recv tap ran before the client
handler, and the tap's state updates and its own "vendor is not found"/
"list is empty" log lines survive on those paths. Consume parity: the
agent returns `ProcessShopList`'s verdict and `ProcessShopList` returns
pass-through when it cannot engage (empty vendor list, no matching shop
list, no matching items), so only a queued replay consumes; the
early-return sits before `CloseGump`/gump build, identical in scope to the
old `return false` (which skipped the whole client handler). The
`g_ShopReceived` wait flag is set before the g_World gate (BuyList 0x74
precedent). No `m_AssistantInjecting` gate: no `CPacket::SendClient`
builder produces 0x9E (the queued replay sends 0x9F, whose SMSGH send row
stays). Row demoted to `UMSG`.

B11 (0x7C Open Menu, the last menu keep) fuses into the client OpenMenu
handler: the client parses the menu once (serial, id, name, gray/color
shape, per-entry graphic/color/name - the gray branch's `Move(4)` skipped
the same 4 bytes the color branch reads, so one entry loop serves both
shapes) and feeds the menu agent from the parsed values (inline
`CMenuItem` mapping, the 0x9E style); the reader-based
`OnPacketReceived(CDataReader&)` became
`OnMenuReceived(serial, id, name, grayMenu, items)`. Record parity: the
menu record (`m_LastMenu` + `m_Items`) is stored on every 0x7C with a
world, exactly as the old tap stored it before deciding; the feed also
keeps the tap's `RemoveLastMenu` detach and its own `g_World` gate, which
is why it sits before the client's world early-out. Only the answer
consumes: a playing macro whose `PlayWaiting({ MT_WAIT_FOR_MENU })` fires
(it returns true only when the current macro step is the waiting step,
rewinds and re-runs it) or the front WaitMenu hook when
`SendChoiceIfMenuExists` answers it (CPacketMenuResponse/
CPacketGrayMenuResponse to the server, reply 0 for cancel) returns false
and the early-return skips the menu gump build - the only downstream work
the handler owns. A macro wait that expires is not an answer: PlayWaiting
then returns false and the client gump builds normally.
`g_MenuReceived` is set before the parse (the old tap set it before the
agent's world gate). Edge parity: 0-item menus record an empty entry list
and build empty gumps; entries whose art resolves to 0-size are still
recorded (the color gump just skips drawing them), as the old tap recorded
every packet entry. No `m_AssistantInjecting` gate: no
`CPacket::SendClient` builder produces 0x7C (the in-client auto-select
rides the 0xFC OCT_SELECT_MENU channel; macro/hook answers send 0x7D,
whose SMSGH send row stays). Row demoted to `UMSG`.

B12 (0xB0 Open Gump, the last keep besides 0xFC) fuses into the client
OpenGump handler: the handler feeds the gump agent at the top, pre-parse,
where both dispatchers enter variable-size rows (Start + 3, the position the
old recv tap used). Reader contract: the agent's parse walks the whole
command/text block, so it reads a stack `CDataReader` over the same view
(`Ptr, End - Ptr`) instead of the shared reader - the 0x6C-style save/restore
is unnecessary because the client's reader is never handed over; the client
parse then re-reads serial/id/x/y/commands/text untouched. Record parity:
`OnPacketReceived` keeps its own `g_World` gate, `RemoveLastGump` detach and
unconditional `m_Items` push, so every 0xB0 with a world stores a record
before any consume decision (multiple gumps with the same serial/id
accumulate; `OnPacketSend` on the kept 0xB1 `SMSGH` row still retires them).
`g_GumpReceived` is set before the feed (the old tap set it before the agent
run). Only two consumes, both skipping the entire client gump build (the old
`return false` skipped the handler's whole downstream: coords cache,
CGumpGeneric construction, command loop, htmlgumps, text lines,
`g_EntryPointer`, `g_GumpManager.AddGump`): a playing macro whose
`PlayWaiting({ MT_WAIT_FOR_GUMP })` fires (the macro replay answers the
server itself), or the front gump hook when `SendGumpChoice` answers it
(0xB1 response to the server, 0xFC/OCT_CLOSE_GENERIC_GUMP_WITHOUT_RESPONSE(100) close-gump injection queued via
SendClient; the response is SendServer-bound so no recv re-entry). A hook
that does not resolve stays queued and the client gump builds normally.
Edge parity: 0-command/0-text gumps record empty lists (the wire counts are
read either way); the bounded reads (`ReadStringChecked`, `ReadWString`) sit
out at `End` without advancing, and the text loop is wire-count bounded, so a
truncated gump parses finite. No `m_AssistantInjecting` gate: no
`CPacket::SendClient` builder produces 0xB0 (compressed 0xDD gumps are
decompressed and re-dispatched in-process by OpenCompressedGump, a
server-data path, not an injection; script gumps build client CGumps
directly and macro/script hook answers call `SendGumpChoice` directly).
Row demoted to `UMSG`; with 0xFC the assistant table now dispatches recv to
one row only.


The parallel assistant packetmanager was dissolved handler-by-handler into
the client's handlers:
the client handler owns parsing and state, assistant logic becomes injected
hooks. Per-handler checklist:

1. **Parse owner**: the client handler parses; never re-parse in the
   assistant. Byte-offset patches from the old pre-parse flow become
   assignments to the already-parsed client variables (verify the old
   raw-buffer offset against the client parse layout first).
2. **Classify** the assistant work: events fired, packet/wire edits, veto
   (drop the packet), pure bookkeeping.
3. **Inject** into the client handler behind `#if defined(XUO_ASSISTANT)`
   at the matching points: events after the client builds its final
   values, veto/early-return before any client side effect, edits before
   the value is used. Keep the assistant's `g_World != nullptr` guards if
   the client handler lacks them. Guarded includes for the assistant
   config/events headers go with the existing `AssistantPackets_Recv`
   include block.
4. **Delete** the assistant handler body, its table row(s) and its
   declaration in `managers/packetmanager.h`.
5. **Verify**: same events (same fields, same fire order relative to the
   veto), same wire edits, veto scope identical to the old `return false`
   (which skipped the whole client handler).

Completed reference case: **DisplayClilocString** (client
`src/Managers/PacketManager.cpp`, ids 0xC1 + 0xCC) — journal record with
system/own-speech flags, the characters-font-color re-hue applied to the
parsed color (the old flow patched the raw buffer at `Start + 10`, which
is the color field in both layouts), and the replace-text/speech-filter
veto. The veto early-return only suppresses client processing (the tap sits
before the client handler dispatch).

Speech tap (0x1C + 0xAE, fused in B3) adds two facts:

- Assistant-injected prints (`CPacket::SendClient` queue → client-table
  dispatch under `m_AssistantInjecting`) are `CPacketToClientSystemPrint`/
  `CharacterPrint`/`TextReplacePrintA|W`, i.e. 0x1C/0xAE. They never ran the
  assistant tap pre-fusion, so both speech hooks gate on `!m_AssistantInjecting`
  — without it every replace-text reprint would be journaled twice and
  re-run through the filters (a reprint matching its own filter loops).
  `DisplayClilocString`'s hook stays ungated: no builder injects 0xC1/0xCC.
- The ASCII tap sits before the client's `font >= g_FontManager.FontCount`
  early-return, matching the pre-fusion order where the assistant journaled
  lines the client then dropped for an invalid font (the assistant's own
  gate was `font >= 10`, which also covers the `0xFFFF` ack probe). The
  unicode tap gates only on the `serial==0 && font/color==0xFFFF` probe.

B7 (death/prompt/profile) completes the BMSGH recv halves:

- Death Screen 0x2C: the g_World-gated bookkeeping (tracker point,
  last-corpse position, 5-minute dead timer) sits after the action parse;
  the `NoDeathScreen` veto early-returns before the client's screen-effect
  block (skipping effect, weather/target reset, drag-gump close, music,
  warmode exit and the death-screen timer) but still prints "You are
  DEAD!". Row demoted to `UMSG` (the send half was a no-op).
- ASCII/Unicode Prompt 0x9A/0xC2: the hook sits pre-parse at the top of the
  client handlers (0x99 shape): both dispatchers enter a variable-size
  handler at `Start+3` (id + length), so `OnPacketReceived` reads
  serial/id from the identical position as the old tap and its
  `*Start == 0xC2` detection is unaffected (Start never moves). A playing
  macro waiting on the prompt consumes the packet; the client then keeps no
  `g_ConsolePrompt`/last-prompt state (its reply path echoes that state).
  Rows demoted to `SMSGH` (the send halves record the prompt round).
- Character Profile 0xB8: profile store (`ProfileReceived`/`Profile`, built
  from the client parse: top + data + bottom lines) plus the one-shot
  `BlockProfile` consume before `CGumpProfile` is added. Row demoted to
  `UMSG` (the send half was a no-op; profile requests are client packets).
- Target cursor 0x6C: the fused block sits at the top of the client handler
  (0x99 shape): both dispatchers enter a fixed-size handler at `Start+1`
  (0x6C is fixed-size 0x13, so no length skip) — the same position the old
  tap dispatched `TargetR` from, so `SetRecvData` (which copies the whole
  packet from `Start` and reads type/cursor-id/cursor-type from offset 1)
  sees identical bytes. A WaitTarget hook answer (CheckHook) or a playing
  macro waiting on the cursor consumes the packet; the client then keeps no
  cursor state and skips the PartyHelper replay. Assistant-published
  replays (`RestoreTarget` → `SendClient(m_CurrentClientTarget)`) arrive
  under `m_AssistantInjecting` and run the client body: the client opens
  its own cursor from the saved bytes. Row demoted to `SMSGHB` (the send
  half keeps recording/intercepting the outgoing 0x6C replies; the
  assistant's own cursor replies go out through the `CPacketTarget*`
  builders, never through the table). B8 left 0xBF/0xFC as the only
  `BMSGH` recv halves; 0xBF followed in B9, the `RMSGH` keep (0xB0) in B12
  - the assistant table dispatches recv to 0xFC only.

## Script API

- The `[Q]/[G]/[D]` classification and the registration table live in
  `script/apiscript.cpp`. Option-name natives cross onto the client's VKI
  ids (e.g. `OptionScaleSpeechDelay` → `VKI_SPEECH_DELAY`).
- The sentinel globals (`self`, `lasttarget`, ...) are strings, matched by
  the serial-resolution helpers; scripts keep working unchanged.

## World model & skill data

- The client's own world objects are the assistant's world model: the
  assistant reads (and keeps its bookkeeping on) the client `g_World`/
  `g_Player` singletons and the global `CGameObject`/`CGameItem`/
  `CGameCharacter`/`CGamePlayer` types; `::g_SkillsManager` is the skill
  table (names, buttons, values, lock state, plus `OldValue` for the
  skills-tab delta column); `::g_CorpseManager` owns the corpse auto-open
  queue. There is no second object model in `src/Assistant/`.
- Packet taps run BEFORE the client handlers.
  Handlers whose state the client already fills (stats, hits/mana/stam,
  healthbar flags, notoriety, body updates, container/equipment updates,
  skill values, object names from talk/mega-cliloc, removals) are pure
  observers: they only fire assistant events, edit the outgoing/incoming
  packet bytes (morph, no-gray-screen, font/color filters) or keep
  assistant-owned state. Everything else on a world object is assistant
  bookkeeping written through the client types (backpack serial, last
  corpse, ignore marks, profile cache/block, party member refresh).
- The client may keep an object a few frames after 0x1D (party members and
  corpses stay on purpose), so find/count/dress paths must tolerate
  briefly-stale objects; the assistant no longer removes objects itself.
- Assistant world helpers that operate on the client objects live in
  `core/worldutil.{h,cpp}` (subtree find/count, equip-layer fallback,
  ignore-mark reset, point distance). `core/world.h` is the seam: it pulls
  the client object headers, and the script harness swaps in stand-in
  declarations (`script/test/worldstubs.h`) for the same names.
- The assistant's skill-cast path (`CSkillManager::Use/TargetUse`) resolves
   names through `g_SkillsManager::Find` (exact name wins, partial fallback,
   button skills only); its own name/number→index map only backs
   `GetIndex`-style lookups.

## Fused globals with non-obvious semantics (keep-with-doc)

- `g_LastUseObject` (assistant copy): keeps the original any-serial
  semantics. The client global of the same name only records items with
  serial >= 0x40000000, which would break scripts tracking non-item uses;
  hence the assistant copy stays assistant-owned.
- Light / personal-light / season / weather mirrors (`g_LightLevel`,
  `g_PersonalLightLevel`, `g_Season`, `g_Weather`): hold the RAW
  pre-filter wire values from packets 0x4E/0x4F/0x65/0xBC. The filter
  toggles restore from these mirrors; they are not the effective
  post-filter values the renderer sees.
- Morph mirrors (`g_MorphGraphic`, `g_OriginalGraphic`): hold the
  pre-morph graphic. Once the morph patch has been applied, the original
  body graphic is not derivable from world state anymore (the client only
  sees patched values), so the assistant must remember it to reset on
  logout.

## Remaining deviations (grep `DEVIATION`, all commented in place)

- No audio playback (`PlayWav` reports and does nothing).
- No speech-mode variants (SayBroadcast goes out as normal speech).
- `LoadProfile` restrictions (script-side profile switching is limited).
- The external UOAM/UOA notify pipes have no in-process equivalent.
- The CLOGGER-only assistant world dump is a no-op (no logger channel).
- `savehotkeys`/`loadhotkeys` target the profile's `Hotkeys.xml`; the
  explicit path argument is not honored.
- Tile-hook categories: numeric graphic, `land`, `any` resolve from the
  client's tiledata; `tree`/`water`/`mine` category lists are not
  available and resolve like an empty list.
- Stay-on-top has no effect (the assistant renders in-game, no OS window
  to keep on top).

## Future dedup & feature proposals

- xuocore CDataReader::ReadWString NUL-scan walks to `buf <= End` and can read up to
  2 bytes past the exclusive End at a buffer boundary; ReadString has the same family
  of loose bounds. Harmless inside the larger recv buffers, worth tightening when the
  datastream is next touched.

Evaluated and deferred; none block the current build.

- **Journal record fusion (client `CTextData`).** The client's
  `CTextData` (src/TextEngine/TextData.h) carries rendering-oriented state
  (type, font, text, owner object pointer, timers) but not the per-entry
  serial/color/flags triple the assistant journal record
  (`model/commonitems/journalmessage` + `script/journal`) exposes to
  scripts. Fusing would mean extending the client's text engine with
  script-facing fields; deferred until the journal API needs more than the
  client engine stores.

- **Shop/trade record fusion.** `vendorshopitem`/`shopitem` and
  `tradewindow` mirror state the client also holds in its shop and trade
  gumps, but the script API reads/writes list entries by index, name,
  amount and price in shapes the client gump state does not preserve
  verbatim. Revisit when the script trade/shop API surface is next
  touched.

- **Prompt state unification.** The client keeps transient prompt state
  (serial, timeout) for its own input gump; the assistant adds hooks on top
  (`managers/promptmanager`) so scripts can wait for and answer prompts.
  A shared prompt-state object would remove the second copy of the serial
  wait; deferred until the prompt flow changes.

- **Shared item-query engine.** `CFindItem` (`model/commonitems/finditem`)
  implements token/range item queries over the world subtree that any
  client-side consumer (scripts, macros, UI filters) could reuse. Moving it
  into a shared client utility is a candidate once a second non-assistant
  caller exists.

- **Distance-helper unification.** The client computes distances between
  world objects (`GameWorld`/object helpers) while the assistant keeps the
  point-based `Distance` in `core/worldutil`. One shared point/serial
  distance helper would remove the duplicate arithmetic; deferred to avoid
  touching client headers for a two-line function.

- **0x78 patch-offset investigation.** The assistant's 0x78 morph patch
  writes at `Start + 7`, which lands on the X field of the packet layout
  (id 1, serial 4, graphic 2, x) rather than the graphic field
  (`Start + 5`); servers reportedly accept both. The offset is commented
  in the client's `src/Managers/PacketManager.cpp`; a capture-backed
  investigation could pin the correct field and whether the graphic field
  needs patching too.

- **StealthSteps counter.** `GamePlayer::StealthSteps` exists but nothing
  increments or reads it; implementing the stealth step countdown (hide/
  reveal after N steps) is feature work, not dedup.


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

## Stubbed-flag mechanics and the zero-latency refresh contract

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

## Zero-latency mode specifics

- Faults never fetch: `trap_stub_fill` serves the page immediately, reports
  each stubbed asset to the backend (`stubbed` → state DB + journal `B`,
  `prefetch` → the demand tier), and the faulting instruction re-executes.
- The demand tier is one worker thread started by `vfs_boot` only when
  zerolat is on and the run is not offline; it heals queued assets through
  `vfs_trap_resolve_asset` (the same blocking resolution, off the fault
  path). Dedup guard per (file, asset); a failed heal drops the guard so a
  later fault can re-enqueue. Offline + zerolat never starts it - stubs are
  served with no network attempts and no retry storm (stubbed pages are
  readable, so they do not refault).
- Mid-session entry patches (packs) make a stub visible to `UopLoadFile` on
  the **next boot**; mid-session tables are already parsed. Loose-file stubs
  are visible immediately (raw-pointer reads).

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
pre-resolve) and the zerolat demand tier still fetch + materialize on demand;
stub payloads are never served (their pages are readable, so nothing faults
into the stub path). Trap-off wins over `--streaming-zerolat` for mapping;
mappings made before the flag flips keep their protection. The HUD shows no
placeholder counts in this mode (nothing is ever marked stubbed).

## HUD

`CVfsStatusHud::Draw` (src/ScreenStages/VfsStatusHud.cpp) is called once per frame
from `CGame::Process` after the screen render - main thread only. It polls
`vfs_hud_stats` (an in-memory snapshot over the boot-owned state; no disk io,
no reconcile - the full `vfs_stats` maps every pack and would hitch the
render) a few times a second, shows activity (rate + done/total), a
"placeholders: N" warning while stubbed assets are in use, and an offline
badge; it fades out ~2 s after the last activity and costs one flag check
when streaming is disabled. It is deliberately not registered in the gump
manager (passive, non-interactive, visible on every screen).

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
  answers every miss asynchronously via `stub_on_miss` + `prefetch`.
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

The zerolat analog (`stub_on_miss`) fills the arena with the stub
payload on a missing blob and reports ready (the miss path answers
instantly); `ensure()` on a resident stub always retries the real blob
first - that is the heal. Boot gates core assets on `vfs_fetched` state, so
a stubbed T0 asset still fails a boot, exactly as zero-latency mode does not
mask a missing core install.

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
registry, the trap backend and the demand tier are all skipped (no traps
exist on the arena backend), T0 granules are ensured resident through the
backend, and the
background tier ensures everything reachable from the store. The state DB
keeps its meaning (fetched = resident this session, stubbed =
placeholder, holes pre-marked), the HUD counters read it unchanged, and
`vfs_stats` skips the on-disk reconcile (there is nothing to reconcile
against). Residency contract: boot re-ensures every T0 granule (blocking -
core assets are small) at EVERY start, restarts included, so the core bytes
are resident again after a restart; T1+ granules are converged by the first
boot's background pass and afterwards repopulate only on access (the
consumer `ensure()`/`stub_on_miss` path - the native memory-mode wires no
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

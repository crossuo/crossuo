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

- M5 optional `.meta` sha256 sidecar verification (plan §10.4), which authenticates
  content regardless of transport; or
- a CA-bundle strategy (ship/bundle roots, enable `CURLOPT_SSL_VERIFYPEER` +
  `CURLOPT_CAINFO`) for https repos.

## Tier refinement: anim.idx vs anim.mul

`anim.idx` (1.7 MB) is T1: LoadCommon requires it, so it is fetched during boot
(§7.2 "sized at boot"). `anim.mul` (190 MB legacy fallback) stays T2: the boot
skeleton prepares it valid-but-unfetched so LoadCommon passes, and content
converges via the idle tier (§7.3 T3).

## Product manifest staleness

The plan is rebuilt only when the product serial changes; until M5 adds
conditional GET / manifest diffing, a re-signed but same-serial manifest is not
re-fetched, and the local copy can lag the repo between boots.

## Embedded T1 tables: converter TODO

The T1 tables are meant to be embedded from recorded runs (§7.5), but the
CSV → C++ converter tool does not exist yet, so recordings cannot be embedded;
this is tracked for M4/M5.

# NOTES — deferred decisions and known limitations (M3)

## MUL/loose streaming is whole-file on first fault

`vfs_trap` resolves loose files (and legacy MULs served as loose plan files)
by fetching and materializing the whole file on the first fault (§10.3
whole-file fallback). Per-index-range streaming (`.idx` entry ranges, batched
64 KB GETs) is deferred: it needs the idx-range covering map plus range-GET
coalescing, and whole-file already unblocks map/statics/anim reads for the
common (smaller) MULs. Statics/texmaps are T1 anyway (§7.3), so the files
that would benefit most from ranges are typically complete before play.

## Async-signal-safety corners of the fault path (pragmatic, per plan §9)

The POSIX `SIGSEGV` handler resolves faults synchronously and deliberately
takes a normal mutex (`s_mutex`), waits on a condvar for in-flight dedup, and
performs a blocking curl fetch *inside the handler*. Everything else in the
hot path is syscall- or lock-free: mmap/mprotect/pwrite plus preallocated
atomic counters, no logging, no allocation (fixed-size claim batches). The
alternatives (deferring resolution to a dedicated thread per fault, or a
fully lock-free registry) trade correctness for latency in ways §9 explicitly
permits us to defer; `--streaming-trap=off` remains the escape hatch when it
lands (M5 CLI work).

Windows uses a first-chance VEH (`EXCEPTION_ACCESS_VIOLATION` only,
`EXCEPTION_CONTINUE_SEARCH` otherwise) with the same resolution core.

## Zero-page degradation until M4 stubs

A failed resolution zero-fills the still-protected sub-ranges of the faulting
page (entry-table and present-block bytes are preserved) and keeps the game
running with zeros - zlib/stored readers degrade naturally (GetData returns
empty / zeros). M4 replaces this with real per-kind stub payloads and marks
pages `vfs_stubbed`; until then the page mark (`vfs_trap_page_mark_at`) is
the placeholder signal. Note the M1 state DB intentionally keeps
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
permanently missing blobs online. M4's stub marking will stop the retry storm.

## EOF-tail reads crash instead of returning zeros

Managed mappings span `ceil(size / page)` pages, but only `[0, size)` is
registered: a read in `[size, page-round-up)` (the tail padding of the last
page) faults, is rejected by the containment check (`offset >= size`) and
chains to the default action - the process crashes with SIGSEGV. This is
deliberate: such a read is an index/entry-table bug, and before the trap it
would have read past the file end anyway; M3 briefly served those reads as
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

## Stub payload model (§6.7, §8)

Stubs are deterministic pure functions of (kind, ul, is_uop, offset): a small
header followed by zeros (the map block repeats one 196-byte `MAP_BLOCK`
pattern instead). Two constraints shape every generator:

- **Stubs live inside the block's planned stored span** (`ml + cl`), never
  `ul`: the sparse layout reserves exactly the on-wire size. Readers take the
  `memcpy` path of `DecompressBlock` (entry advertises `Flags=0`,
  `CompressedSize=min(ul, cl)`, plan `DecompressedSize`), which pads the tail
  with zeros - exactly the stub's semantics. The entry keeps the plan's
  `Hash`/`DecompressedSize`. Fixed in review: `vfs_store_pack_asset_present`
  never mistakes such a patch for the real thing (see "Fixed in review" M2),
  and stub entries are never promoted to fetched by reconcile.
- **Headers are all-or-nothing.** A truncated header would let the game's
  parsers walk bytes beyond the stub inside the mapped file (run-length
  overflows in `GetArtPixels`/`GetGumpPixels`). Below `vfs_stub_min_payload`
  the generator writes nothing and the M3 zero-fill serves instead - those
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

## Stubbed-flag mechanics and the zero-latency refresh contract (§6.8)

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
  (fixed in review M1: `AnimationRefreshStub` is driven by the trap's healed
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

Three blockers from the M4 review, closed in place:

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
- **M1 - the animation hot-swap hook was dead code.**
  `AnimationRefreshStub` required the state DB byte to still be `stubbed`
  **and** the trap asset to be healed, but the heal itself moves the DB byte
  to `fetched` before the trap marks the asset present - the intersection was
  empty and cached stub frames were never dropped. Fix: the hook is driven by
  the trap's healed query (`vfs_refresh_block`) plus a one-shot marker per
  animation group (`s_anim_refreshed_groups`), dropping the group whole (all
  directions share the group's block) exactly once.
- **M2 - reconcile permanently promoted ct=0 stubs to fetched.**
  `trap_stub_patch_entry` writes `Flags=0`/`CompressedSize=min(cl, ul)`, so a
  stored (ct==0) asset's patched entry is byte-identical to the plan and
  `vfs_store_pack_asset_present` reported it present - reconcile upgraded
  stubbed (and pending, after a heal crash) to fetched on that alone, making
  the placeholder bytes permanent. Fix: reconcile never promotes `stubbed`
  from on-disk presence at all (only the heal path - the materialize write
  plus its journal `F` - promotes), and pending promotion requires the
  reconcile-grade presence check (`blobs_dir` given): ct==1 stub patches are
  rejected by entry shape (plan says zlib, patch says stored), ct==0 patches
  by content (stored head vs blob head; missing blob = not present). The
  pure shape check stays the trap's boot-time visibility rule - a stub
  patched by a previous session must stay visible to `UopLoadFile`.

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
  truth until M5 teaches verify/seal about `vfs_stubbed`.
- **Gump/art overlap**: `artLegacyMUL.uop` serves land tiles with the same
  hash-space as statics; the run-header stub leaks two nonzero u16s into the
  raw-land pixel read (2 of 1012) - visually a couple of near-black specks
  on an otherwise transparent placeholder tile.

## M4 review residuals (latent, non-blocking)

- Persisted ct=0 stub patches read as boot-present via pure-shape checks
  (`vfs_trap.cpp` boot visibility): such a gump stays empty until remap even
  though the DB honestly says `stubbed`. Pre-existing M3 behavior; heal paths
  and reconcile are post-M4-fix honest.
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

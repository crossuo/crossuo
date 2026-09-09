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

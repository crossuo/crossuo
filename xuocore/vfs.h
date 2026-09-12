// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs: public VUOI core API (plan/state/materialize/verify over a local blob
// store; boot, fetch, trap and stub APIs). The install dir produced here is
// the future uo_data_init() target.

#pragma once

#include <atomic>
#include <common/fs.h>
#include <stdint.h>

#include "uop.h"
#include "vfs_manifest.h"
#include "vfs_state.h"
#include "vfs_store.h"

struct VfsStats
{
    size_t files_total = 0;
    size_t files_done = 0;     // files with every expected asset present
    size_t bytes_total = 0;    // planned bytes (pack tables + blocks, loose files)
    size_t bytes_done = 0;     // bytes confirmed in place
    bool online = false;       // fetch engine connected to a repo
    uint64_t stubs_served = 0; // faults answered with a stub payload
    uint64_t stubs_active = 0; // assets currently in the stubbed state (HUD)
};

struct vfs_params
{
    bool enabled = false; // streaming install mode; false: no-op module
    bool offline = false; // never hit the network; cache/stubs only
    // escape hatch (--streaming-trap=off): managed files map
    // without hole protection - pointer reads of holes return the sparse
    // file's zeros; ensure/pre-resolve paths still fetch on demand
    bool traps = true;
    uint32_t workers = 0; // download workers; 0: default (cpu-1, cap 8)
    fs_path blobs_dir;    // local blob store root (mirrors the remote URL tree)
    fs_path install_dir;  // materialized game files
    fs_path state_dir;    // state DB; default: <install_dir>/cache/state
    // product manifest; default: <blobs_dir>/uo-legacyrelease.prod, resolved
    // against the blobs root when the given value is not an existing path
    fs_path product_file;
    // where the product manifest is downloaded from when missing;
    // empty: the official EA legacyrelease patch server URL
    astr_t product_url;
    // deployment-time asset-access recorder (--streaming-record): appends
    // every resolved asset to a CSV that seeds the embedded T1 order;
    // empty: disabled
    fs_path record_path;
    astr_t record_scenario; // --streaming-record-scenario (default "default")
    // granule store backend: the native sparse materializer (default) or
    // the memory arena - web groundwork, where granules live in heap arenas
    // holding decompressed payload bytes instead of materialized files; on a
    // native build the arena is a memory-mode (no install files appear)
    vfs_store_backend_kind store_backend = vfs_store_backend_sparse;
    // product subset filter for the plan build: none is the full native
    // view, the web subset drops the large legacy patterns (anim*.mul,
    // Music/**, facet*, verdata). Persisted in plan.bin
    vfs_subset subset = vfs_subset_none;
};

void vfs_set_params(const vfs_params &params);
const vfs_params &vfs_get_params();
const char *vfs_install_path();
bool vfs_enabled();

// plan lifecycle: load the persisted plan when it matches the product
// manifest serial, (re)build and persist it otherwise
bool vfs_plan_ensure(VfsPlan &out_plan);
bool vfs_plan_rebuild(VfsPlan &out_plan);

// materialize everything reachable from the blob store (idempotent, offline);
// returns false only on io failures or unusable blobs, missing blobs are not
// fatal (the fetch engine downloads them later)
bool vfs_materialize(vfs_materialize_stats *stats);

// verify materialized files against plan and blobs; per-file statuses in
// `out`; returns the number of corrupt assets (0 when clean, -1 when the
// plan cannot be built), which is what makes this a hard failure
int vfs_verify(vfs_verify_result *out);

// aggregate progress over the current plan and state DB
void vfs_stats(VfsStats *out);

// in-memory progress snapshot for the HUD: reads the boot-owned state
// and lock-free counters only - no disk io, no reconcile - so the game loop
// can poll it at a few Hz. zeros while no boot ran.
void vfs_hud_stats(VfsStats *out);

// arena residency probe (tests + web diagnostics): the tracked/resident
// granule counts of the params-selected backend; zeros when the sparse
// backend is active or no boot holds an arena
void vfs_arena_residency(size_t *out_granules, size_t *out_resident);

// ---------------------------------------------------------------------------
// boot: load/refresh the plan, then a blocking core-tier (T0)
// fetch+materialize (with progress), then the background tier (T1); returns
// false on fatal errors (offline mode
// with an insufficient store, unreachable repos, failed core assets)
// ---------------------------------------------------------------------------

bool vfs_boot(void (*progress)(float done01, const char *label));
// the game calls this once the first frame rendered: boot-critical work is
// over and the background worker may start post-frame convergence (store ->
// install materialization). Until then the worker only serves the boot tiers
void vfs_notify_first_frame();
// headless tools (vuoi): wait for the background tier to finish instead of
// leaving it running; the game never calls this (it must not block the loop).
// Implies vfs_notify_first_frame: the wait covers convergence too
void vfs_boot_wait();
// full install: fetches the blob of EVERY planned
// asset (all tiers) into the store; blocking, optional progress per tail.
// Requires a running boot (repos + engine); false when offline or idle.
// The caller materializes and verifies afterwards (xuoi --full).
bool vfs_fetch_all(void (*progress)(float done01, const char *label));
// stops the fetch engine and the background tier, saves the state DB
void vfs_shutdown();

// ---------------------------------------------------------------------------
// managed mappings: hole-paged mappings with
// fault-driven streaming. vfs_map_file routes install files known to the
// plan through vfs_trap (unreadable holes, first touch fetches the covering
// block); every other path - and the whole path when streaming is disabled -
// is a plain fs_map with identical semantics. vfs_ensure_range pre-resolves
// a byte range synchronously without faulting (GetData/GetRaw call it; the
// trap remains the safety net). No-op when streaming is disabled.
// ---------------------------------------------------------------------------

bool vfs_map_file(const fs_path &path, uint8_t *&out_start, size_t &out_size);
// unmap counterpart; routes through the trap when the base is registry-owned
void vfs_unmap_file(uint8_t *start, size_t size);
// makes [offset, offset + length) of a mapped managed base readable (fetch +
// materialize + flip, deduplicated); false when resolution failed hard
bool vfs_ensure_range(uint8_t *base, size_t offset, size_t length);
// true when the byte at `offset` holds real materialized data (sealed
// or flipped pages). Unmanaged/native addresses are always ready. The cheap
// check behind stub-refresh decisions and diagnostics
bool vfs_is_ready(uint8_t *base, size_t offset);

// ---------------------------------------------------------------------------
// stubs and hot-swap: the trap serves syntactically
// valid placeholder payloads for missing ranges (zeros only when the kind has
// no payload content); zero-latency mode adds instant stubs + background
// healing. Cached game objects decoded from a stub go stale when the real
// bytes land - the refresh helpers below detect that and let the caller drop
// the cache so the next load decodes the real asset. All of them are no-ops
// when streaming is disabled, and refreshes only ever fire for objects the
// index build marked Stubbed.
// ---------------------------------------------------------------------------

// assets served from a stub payload since boot (HUD/telemetry counter)
uint64_t vfs_stub_count();
// true when the asset whose block contains `addr` was served a stub and its
// real bytes have since materialized (one-shot per check: the caller decides
// what to drop and reloads; the object's Stubbed flag must be cleared by it)
bool vfs_refresh_block(const uint8_t *addr);

struct CIndexObject;

// CIndexObject flavor: checks `io->Stubbed` + vfs_refresh_block(io->Address);
// when healed, releases the cached sprite through `destroy` (game-supplied,
// e.g. DestroySprite), clears UserData and the flag, and returns true so the
// caller falls through to the normal load. One-liner contract at every
// Execute*/cache entry point.
bool vfs_refresh_index(CIndexObject *io, void (*destroy)(CIndexObject *obj));

// index-build markers: true when the pack asset with `uop_hash` (or
// the managed loose file mapped at `base`) was served a stub payload. The
// index build marks the CIndexObject so healed bytes hot-swap later. Always
// false when streaming is disabled.
bool vfs_asset_is_stubbed(uint64_t uop_hash);
bool vfs_file_is_stubbed(const uint8_t *base);

// planned payload bytes of one entry inside a managed pack mapping, resolved
// by the entry's uop name hash: the on-disk table entry of a block that was
// never materialized carries Offset == 0, but the plan fixes its byte range
// up front. Baking the returned address into the game's index exposes the
// entry, so raw-pointer reads fault into the trap and stream on demand.
// False when streaming is disabled, untracked, unknown to the plan, or a
// hole (reserved slot with no bytes - the index keeps Address == 0).
bool vfs_asset_by_hash(
    const uint8_t *mapping_base,
    uint64_t uop_hash,
    uint64_t *out_offset,
    uint32_t *out_size);

// full entry reconstruction for hashes the mapped table does not carry yet
// (their block never materialized, so UopLoadFile skipped the slot): fills
// `out_entry` exactly as the materialized pack's table would describe it
// (planned offset, sizes, checksum, materialized encoding) so the caller can
// index and decode the block from its planned address. `out_owner` (optional)
// receives the owning plan file's name ("AnimationFrame2.uop"). False when
// streaming is disabled or the hash is unknown to the plan.
bool vfs_asset_entry_by_hash(uint64_t uop_hash, const char **out_owner, UopFileEntry *out_entry);

// per-file bulk form of the markers above (perf): a mapped pack's plan
// file resolves once per index build - `vfs_file_state_base` returns the
// state DB base index of the plan file mapped at `mapping_base` (SIZE_MAX
// when untracked or streaming is disabled), and `vfs_asset_stubbed_at`
// answers the per-entry question with a single state read instead of a hash
// lookup + a plan walk per object
size_t vfs_file_state_base(const uint8_t *mapping_base);
bool vfs_asset_stubbed_at(size_t base, size_t asset_index);

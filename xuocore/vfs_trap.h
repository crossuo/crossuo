// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_trap: page-guard mappings with fault-driven resolution (VUOI plan §6.5,
// §9). Managed files are mapped with their not-yet-materialized byte ranges
// protected (PROT_NONE / PAGE_NOACCESS); the first access to such a hole
// faults, the handler resolves the covering asset block (fetch blob to the
// store + the M1 write path), flips the page readable and the faulting
// instruction re-executes - so every raw-pointer read path streams without
// modification (§5.1).
//
// Coverage is tracked per page: a page is kept protected until every block
// overlapping it is present (page-straddling blocks flip only when fully
// covered). Sealed files map plain readable and never fault.
//
// Lock order (§9): trap_lock -> store_lock (vfs.cpp state mutex) ->
// fetch_lock. Resolution runs in three phases - lookup under trap_lock,
// fetch/materialize with NO trap lock held, flip under trap_lock - so a
// faulting thread never deadlocks against the background tier. Fetch
// completion never calls back into the trap.

#pragma once

#include <stdint.h>

#include <common/fs.h>

#include "vfs_manifest.h"
#include "vfs_store.h"

// one covering block handed to the backend for resolution
struct vfs_trap_block
{
    uint32_t file_index = 0;
    uint32_t asset_index = 0;
    const char *tail = nullptr; // blob store / URL tail ("" for local-only runs)
    uint64_t expected = 0;      // exact on-wire size (0: unknown)
    bool is_loose = false;      // loose files resolve whole-file (§10.3)
};

// the resolution backend: fetch the blob into the store when missing, then
// run the M1 write path for exactly this asset; true when the bytes are in
// place afterwards. Called with no trap lock held and must not call back
// into vfs_trap (lock order above); the default backend fails every block,
// which degrades resolution to the zero-fill path (M4 replaces with stubs).
struct vfs_trap_backend
{
    void *context = nullptr;
    bool (*ensure)(void *context, const vfs_trap_block *block) = nullptr;
};

void vfs_trap_set_backend(const vfs_trap_backend &backend);

struct vfs_trap_stats
{
    uint64_t faults = 0;     // handled faulting accesses inside managed ranges
    uint64_t resolved = 0;   // faults satisfied with real bytes
    uint64_t zeroed = 0;     // faults degraded to a zero-filled page
    uint64_t dedup_waits = 0; // faults that waited on an in-flight resolution
    uint64_t chained = 0;    // faults outside managed ranges (real bugs)
    uint64_t flips = 0;      // pages flipped from protected to readable
};

// lock-free counters; drained by the uninstaller when VFS_FAULT_COUNTERS is
// set in the environment, readable anytime (tests)
void vfs_trap_stats_get(vfs_trap_stats *out);

// maps a managed install file with hole pages protected; returns nullptr on
// failure. Sealed files (every expected asset already present) map plain
// readable and skip trap setup entirely. `file_index` selects the plan file;
// the caller guarantees path == <install_dir>/<plan file name> and that the
// plan outlives the mapping. `file_states` (optional) carries the state DB
// byte per plan-file asset: pack presence is always the on-disk entry table
// (self-contained truth), loose files have no partial state on disk, so
// their single asset is present only when the state DB says fetched.
uint8_t *vfs_trap_map(
    const fs_path &path,
    const VfsPlan &plan,
    size_t file_index,
    const uint8_t *file_states,
    size_t *out_size);

// unregisters and unmaps a registry-owned base; false when not ours (the
// caller falls back to fs_unmap). Waits (bounded) for resolutions in flight
// against the file; when one outlives the wait the mapping turns zombie: it
// stays mapped and registered for the process lifetime (never unmapped or
// freed, so the resolver can never run against freed state) and the memory
// is reclaimed by the os at exit
bool vfs_trap_unmap(uint8_t *base);

// true when [offset, offset+length) lies fully within a registered mapping
bool vfs_trap_contains(const uint8_t *base, uint64_t offset, uint64_t length);

// true when every byte of [offset, offset+length) is already readable (no
// resolution, no fetch) - the cheap check behind pre-resolve decisions
bool vfs_trap_range_present(const uint8_t *base, uint64_t offset, uint64_t length);

// synchronous resolution of [offset, offset+length): fetches + materializes
// every absent covering block (deduplicated with in-flight work) and flips
// the covered pages; on failure the requested pages are zero-filled and the
// file's assets are marked failed (M4 serves real stubs). Returns false only
// when the range is not registered.
bool vfs_trap_resolve_range(uint8_t *base, uint64_t offset, uint64_t length);

// marks [offset, offset+length) of a mapping present and re-checks page
// coverage, flipping every fully-covered page to readable; used after the
// backend materialized bytes behind the mapping
void vfs_trap_flip(uint8_t *base, uint64_t offset, uint64_t length);

// page mark for the page holding `offset` (M4 consumes the zeroed markers as
// stub-placement points and the failed ranges for retry scheduling)
enum vfs_trap_page_mark : uint8_t
{
    vfs_trap_mark_none = 0,   // protected hole (or beyond the file end)
    vfs_trap_mark_real = 1,   // materialized bytes
    vfs_trap_mark_zeroed = 2, // degraded zeros (fetch failed; M4 stub point)
};
vfs_trap_page_mark vfs_trap_page_mark_at(const uint8_t *base, uint64_t offset);

// system page size (the mapping/protection granularity)
size_t vfs_trap_page_size();

// installs the platform fault handler (POSIX SIGSEGV with SA_SIGINFO saving
// the previous action; Windows VEH as first handler); idempotent, and called
// automatically by the first vfs_trap_map
bool vfs_trap_install();

// raw fault-context discriminator: true when `context` describes a WRITE
// access - POSIX: the ucontext_t* a SA_SIGINFO handler received (x86-64
// REG_ERR write bit); Windows: the EXCEPTION_RECORD* of an access violation
// (ExceptionInformation[0] == 1). Unknown/null contexts report a read:
// writes are never resolved (that would livelock), they chain to the
// previous handler/default action. Test/diagnostic hook for the write-fault
// chaining behavior.
bool vfs_trap_fault_is_write(void *context);

// removes the handler and forgets the registry; mappings are never unmapped
// here - the game may still hold CMappedFile base pointers at exit and
// unmaps them afterwards through the plain fs_unmap fallback, so the mapping
// memory deliberately leaks for the process teardown rather than risking a
// use-after-unmap. Resolutions in flight get a bounded (5s) grace; mappings
// still resolving then turn zombie (kept alive and mapped, logged loudly) so
// no resolver is ever left holding a freed mapping. Idempotent.
void vfs_trap_shutdown();

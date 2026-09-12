// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_store_arena: the granule-resident memory backend (web groundwork). One
// granule == one plan asset (a UOP pack part or a loose file); idx-range
// clusters do not exist in the plan model yet (see docs/NOTES.md). No files
// are materialized and no traps exist: a granule is
// either resident (heap buffer with the asset's payload bytes) or it is not,
// and reads go through explicit ranges into a caller-supplied buffer.
//
// This is the store a web build runs on (wasm has no mmap and no demand
// paging, and the linear-memory budget forbids multi-GB materialized files).
// It is also useful natively as a memory-mode - tools
// and tests can stream a product through RAM with no install dir at all.
//
// Payload semantics: arenas hold the asset's PAYLOAD bytes (decompressed for
// zlib assets - the same bytes vfs_store_asset_inflate produces for the
// loose writer; raw for stored ones). Pack header/entry tables are NOT
// resident: on Web the UOP table synthesis produces in-memory tables
// and the metadata bytes would be dead weight in a 2 GB budget.
//
// Consumer liveness (THE arena rule): eviction can drop a resident
// granule at any ensure/evict that hits the budget. The game layer must pin
// or refresh consumers of arena bytes - natively that is the refresh
// machinery (vfs_refresh_index / vfs_refresh_block: destroy the cached
// sprite, clear the flag, reload on the next frame). The hook is named here
// on purpose: the web port wires the same DestroySprite path.
//
// Eviction policy: LRU on last access (ensure/read touch the clock). Sealed
// granules stay EVICTABLE like all others - bytes are
// always recoverable from the store/blob, and arena mode pairs with the
// no-traps configuration where a later ensure() simply re-materializes them.
// A granule with an in-flight ensure is never evicted (its commit would
// resurrect bytes the budget asked to drop).
//
// `#if defined(__EMSCRIPTEN__)` hooks (documented, not added - verifiable
// natively only without them): persistence (browser Cache-API/OPFS granule
// store) wraps
// ensure()/evict(); index baking (CIndexObject.Address = arena slot) sits on
// top of read(); neither changes this interface.

#pragma once

#include <condition_variable>
#include <map>
#include <mutex>
#include <stdint.h>
#include <vector>
#include "vfs_store.h"

// arena residency budget default (sized for wasm32 linear-memory headroom)
#define VFS_ARENA_MAX_BYTES_DEFAULT (256ull << 20)

struct vfs_store_arena_params
{
    vfs_store_params store; // the blob store root (blobs_dir; nothing else is used)
    // residency budget: ensure() evicts LRU granules down to it after every
    // commit; evict(bytes_target) enforces it explicitly. A single granule
    // larger than the budget stays resident (there is nothing smaller to
    // keep instead - the budget is a target, not a hard cap)
    uint64_t max_bytes = VFS_ARENA_MAX_BYTES_DEFAULT;
    // blocking-ensure fetch bound (mirrors the trap's VFS_TRAP_FETCH_TIMEOUT_MS)
    uint32_t fetch_timeout_ms = 30 * 1000;
};

struct vfs_arena_stats
{
    size_t granules = 0; // tracked non-hole plan assets
    size_t resident = 0; // granules currently resident
    uint64_t bytes_resident = 0;
    uint64_t evictions = 0; // granules dropped by eviction so far
};

class vfs_store_arena final : public vfs_store_backend
{
public:
    explicit vfs_store_arena(const vfs_store_arena_params &params);
    ~vfs_store_arena() override;

    // vfs_store_backend: the native parity battery runs on this object
    // (ensure/is_ready/read round-trip, prefetch, LRU eviction, seal)
    bool open(const VfsPlan &plan) override;
    bool ensure(const VfsAsset &asset, bool block) override;
    void prefetch(const VfsAsset &asset, int prio) override;
    void evict(size_t bytes_target) override;
    bool is_ready(const VfsAsset &asset) const override;
    void seal_check() override;

    // explicit-range read of resident payload bytes (no traps: pages do not
    // exist here - the caller provides the buffer). False when the granule
    // is not resident or [offset, offset + length) leaves the payload.
    bool read(const VfsAsset &asset, size_t offset, uint8_t *dst, size_t length);

    // drops one granule (update flow: a changed asset re-ensures on
    // demand; the caller resets the state byte and drops the cached blob)
    void invalidate(const VfsAsset &asset);

    // seal bookkeeping view (tests, web diagnostics): the plan file reached
    // 100% granule residency at the last seal_check(). Eviction does not
    // clear the mark (see the header: sealed granules stay evictable).
    bool file_sealed(size_t file_index) const;

    vfs_arena_stats stats() const;

private:
    struct vfs_arena_granule
    {
        uint32_t file_index = 0;
        uint32_t asset_index = 0;
        bool hole = false;     // blockless entry: trivially ready, no bytes
        bool inflight = false; // an ensure() is loading it (never evicted)
        bool resident = false;
        uint64_t last_access = 0; // LRU clock stamp
        std::vector<uint8_t> bytes;
    };

    bool locate(const VfsAsset &asset, size_t *granule_index) const;
    // assumes m_mutex held; drops LRU granules until bytes <= target
    void evict_locked(uint64_t bytes_target);
    void touch_locked(vfs_arena_granule &granule);

    vfs_store_arena_params m_params;
    const VfsPlan *m_plan = nullptr;
    std::map<uint64_t, size_t> m_pack_index; // uop_hash -> granule
    std::map<astr_t, size_t> m_loose_index;  // loose name -> granule
    std::vector<vfs_arena_granule> m_granules;
    std::vector<bool> m_sealed; // per plan file (seal_check)
    uint64_t m_bytes_resident = 0;
    uint64_t m_evictions = 0;
    uint64_t m_clock = 0; // LRU stamp source
    mutable std::mutex m_mutex;
    std::condition_variable m_cv; // in-flight ensure handoff
};

// the memory backend (factory-selected through vfs_store_backend_create)
vfs_store_arena *vfs_store_backend_arena_create(const vfs_store_arena_params &params);

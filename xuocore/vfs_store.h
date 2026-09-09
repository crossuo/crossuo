// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_store: the materializer (VUOI plan §6.3, §10.1, §10.2).
//
// Packs are assembled as sparse files with the complete header + section +
// entry table written up front and blocks written at their planned offsets as
// blobs become available ("planned-offset sparse layout"):
//   [UopHeader][UopSection][UopFileEntry table][blocks...]
//   block_i = [ml bytes of synthesized metadata][payload blob]
// Unwritten entries keep Offset == 0 in the on-disk table (UopLoadFile skips
// them) and get an in-place 8-byte Offset patch when their block lands, so a
// partially materialized pack loads and exposes exactly its present assets.
// Blockless manifest entries ("holes") reserve a table slot but no bytes and
// are never written.
//
// Every materialization step is idempotent: re-running with the same plan and
// blob store is a no-op, and an interrupted run resumes from the state DB.

#pragma once

#include <stdint.h>
#include <vector>

#include <common/fs.h>

#include "vfs_manifest.h"
#include "vfs_state.h"

struct vfs_store_params
{
    fs_path blobs_dir;
    fs_path install_dir;
    fs_path state_dir; // journal/state home; defaults to <install_dir>/cache/state
};

struct vfs_materialize_stats
{
    size_t assets_total = 0;
    size_t assets_present = 0; // already materialized before this run (skipped)
    size_t assets_fetched = 0; // newly materialized by this run
    size_t assets_missing = 0; // blob not in the store (M2: fetch list)
    size_t assets_hole = 0;    // reserved, blockless entries (nothing to write)
    size_t assets_failed = 0;  // io errors or unusable blobs
    uint64_t bytes_written = 0;
};

enum vfs_verify_status : uint8_t
{
    vfs_verify_ok = 0,            // bytes present and validated
    vfs_verify_missing_blob = 1,  // asset not in the store and not in the file
    vfs_verify_missing_block = 2, // blob available but the file hole is open
    vfs_verify_corrupt = 3,       // bytes present but sizes/flags/zlib mismatch
};

struct vfs_verify_asset
{
    uint64_t uop_hash = 0;
    astr_t name;                    // loose-file name ("" for pack parts)
    uint8_t status = vfs_verify_ok; // vfs_verify_status
};

struct vfs_verify_file
{
    astr_t name;
    bool is_uop = false;
    bool sealed = false;   // every expected asset present
    size_t counts[4] = {}; // by vfs_verify_status
    std::vector<vfs_verify_asset> assets;
};

struct vfs_verify_result
{
    std::vector<vfs_verify_file> files;
    bool corrupted = false; // any vfs_verify_corrupt anywhere
};

// Materializer backend interface (VUOI plan §6.3). granule == one UOP asset
// or one loose file in M1; M2 adds the fetch engine behind ensure() and the
// Web build swaps this implementation for the heap-arena store (§17.3).
struct vfs_store_backend
{
    // prepare granule bookkeeping from the plan
    virtual bool open(const VfsPlan &plan) = 0;
    // make the asset's bytes readable; false => not ready (caller may retry,
    // enqueue a fetch (M2) or serve a stub (M4)). M1 materializes from the
    // local blob store only, synchronously.
    virtual bool ensure(const VfsAsset &asset, bool block) = 0;
    virtual void prefetch(const VfsAsset &asset, int prio) = 0;
    virtual void evict(size_t bytes_target) = 0; // residency control (web only)
    virtual bool is_ready(const VfsAsset &asset) const = 0;
    virtual void seal_check() = 0; // mark files that reached 100% presence
    virtual ~vfs_store_backend() = default;
};

// the M1 native backend: sparse planned-offset files materialized from the
// local blob store (owns its own state DB and journal under params)
vfs_store_backend *vfs_store_backend_sparse_create(const vfs_store_params &params);

// batch drivers over the sparse store (used by the CLI, tests and boot):
// states/journal are owned by the caller so runs compose (load -> reconcile
// -> materialize -> save). `items` (optional) restricts the run to those
// (file_index, asset_index) pairs - the fetch-driven boot path materializes
// exactly what it downloaded, through the same write path.
struct vfs_store_item
{
    uint32_t file_index = 0;
    uint32_t asset_index = 0;
};

bool vfs_store_materialize(
    const VfsPlan &plan,
    const vfs_store_params &params,
    std::vector<uint8_t> &states,
    vfs_journal *journal,
    vfs_materialize_stats *stats,
    const std::vector<vfs_store_item> *items = nullptr);

// creates the boot skeleton from the plan without fetching or writing any
// block: pack headers + tables (blocks stay holes) and loose files sized to
// their planned size (sparse). Boot-critical files exist right after the
// blocking tier, valid-but-unfetched (plan §7.2, §7.3).
bool vfs_store_prepare(const VfsPlan &plan, const vfs_store_params &params);

// on-disk truth check for one pack asset (shared with the state reconcile)
bool vfs_store_pack_asset_present(
    const uint8_t *start, uint64_t file_size, size_t asset_index, const VfsAsset &asset);

// ---------------------------------------------------------------------------
// trap support (M3, plan §6.5): page-coverage computation over the plan's
// deterministic layout. Blocks are [file_offset, file_offset + ml + payload)
// - exactly the bytes the M1 write path puts down for the asset. Indices
// appended to `out` are ascending (the layout packs blocks contiguously).
// ---------------------------------------------------------------------------

void vfs_store_blocks_overlapping(
    const VfsPlan::VfsFile &file,
    uint64_t start,
    uint64_t end,
    std::vector<uint32_t> *out);

// materializes exactly one asset through the M1 write path (the same code
// path as the batch driver, restricted to one item), assuming the blob is
// already in the store (the trap fetches first). The caller owns `states`
// (and journal); returns true only when the asset bytes are confirmed in
// place afterwards (state == fetched).
bool vfs_store_rematerialize_asset(
    const VfsPlan &plan,
    const vfs_store_params &params,
    std::vector<uint8_t> &states,
    vfs_journal *journal,
    uint32_t file_index,
    uint32_t asset_index);

// full verify pass over the plan; returns the number of corrupt assets (0
// when clean) and fills `out` (optional) with the per-asset details
int vfs_store_verify(const VfsPlan &plan, const vfs_store_params &params, vfs_verify_result *out);

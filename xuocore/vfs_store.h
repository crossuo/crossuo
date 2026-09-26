// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_store: the materializer.
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
    // quiet the per-run summary log (post-frame convergence batches)
    bool quiet = false;
    // feed the access recorder (trap-path materializations only: prefetch
    // and convergence runs are cache-warming, not usage)
    bool record = true;
};

struct vfs_materialize_stats
{
    size_t assets_total = 0;
    size_t assets_present = 0; // already materialized before this run (skipped)
    size_t assets_fetched = 0; // newly materialized by this run
    size_t assets_missing = 0; // blob not in the store (fetch-engine work list)
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
    // the state DB says the mapped bytes are a placeholder payload.
    // Reported as vfs_verify_missing_block (the placeholder never counts as
    // present *or* corrupt) with this flag set so tools can label it "stub"
    bool stubbed = false;
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

// Materializer backend interface. granule == one UOP asset or one loose
// file; the fetch engine hides behind ensure()/prefetch() and the
// Web build swaps this implementation for the heap-arena store (arenas
// hold decompressed granule payloads, no materialized files).
struct vfs_store_backend
{
    // prepare granule bookkeeping from the plan
    virtual bool open(const VfsPlan &plan) = 0;
    // make the asset's bytes readable; false => not ready (caller may retry,
    // enqueue a fetch or serve a stub). The sparse backend materializes from
    // the local blob store only, synchronously.
    virtual bool ensure(const VfsAsset &asset, bool block) = 0;
    virtual void prefetch(const VfsAsset &asset, int prio) = 0;
    virtual void evict(size_t bytes_target) = 0; // residency control (web only)
    virtual bool is_ready(const VfsAsset &asset) const = 0;
    virtual void seal_check() = 0; // mark files that reached 100% presence
    virtual ~vfs_store_backend() = default;
};

// the native backend: sparse planned-offset files materialized from the
// local blob store (owns its own state DB and journal under params)
vfs_store_backend *vfs_store_backend_sparse_create(const vfs_store_params &params);

// backend selector: the native sparse materializer above, or
// the granule-resident memory arena (web groundwork; natively a memory-mode,
// see vfs_store_arena.h)
enum vfs_store_backend_kind : uint8_t
{
    vfs_store_backend_sparse = 0,
    vfs_store_backend_arena = 1,
};

// selects a backend by kind (vfs_params.store_backend); `params` feeds both
// flavors (the arena only reads the blob store: it never writes files)
vfs_store_backend *vfs_store_backend_create(
    vfs_store_backend_kind kind,
    const vfs_store_params &params);

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
// blocking tier, valid-but-unfetched (the game loads tables/indices only).
bool vfs_store_prepare(const VfsPlan &plan, const vfs_store_params &params);

// true when the loose file at `path` holds the asset's real payload: the
// leading bytes must match the blob (decompressed for zlib assets) when the
// store root is known. This is what separates a real block from the untouched
// boot skeleton - a sparse file of the right size with no content - so a
// freshly sized skeleton never promotes to fetched once its blob arrives
// (full-install convergence).
bool vfs_store_loose_asset_content_ok(
    const fs_path &path,
    const VfsAsset &asset,
    const fs_path &blobs_dir);

// on-disk truth check for one pack asset (shared with the state reconcile
// and the trap's boot presence). `blobs_dir` opts into the reconcile-grade
// strictness (see the implementation): ct==0 stub patches only separate from
// real blocks by content, so the stored head is checked against the blob
// (empty root: entry-shape semantics only)
bool vfs_store_pack_asset_present(
    const uint8_t *start,
    uint64_t file_size,
    size_t asset_index,
    const VfsAsset &asset,
    const fs_path &blobs_dir = fs_path());

// ---------------------------------------------------------------------------
// in-memory payload path (web groundwork): the write-path knowledge both
// backends share. The sparse writer puts [metadata][stored payload] on disk
// (zlib blobs inflate right before the write); the arena keeps payload bytes
// only - the loose writer and the arena produce the same payload from the
// same blob, so the format lives in exactly one place.
// ---------------------------------------------------------------------------

// pure payload step: stored assets pass the blob through, zlib assets
// inflate (the same uncompress the loose writer always ran). Returns 0 with
// `out` = the payload, 2 when the blob cannot supply it (zlib failure).
int vfs_store_asset_inflate(
    const VfsAsset &asset,
    const std::vector<uint8_t> &blob,
    std::vector<uint8_t> *out);

// blob read + inflate in one step (the arena's ensure path). Returns 0 with
// `out` = the payload, 1 when the blob is missing (out untouched) and 2 when
// it is unusable (size mismatch or zlib failure)
int vfs_store_asset_payload(
    const fs_path &blobs_dir,
    const VfsAsset &asset,
    std::vector<uint8_t> *out);

// ---------------------------------------------------------------------------
// trap support: page-coverage computation over the plan's
// deterministic layout. Blocks are [file_offset, file_offset + ml + payload)
// - exactly the bytes the write path puts down for the asset. Indices
// appended to `out` are ascending (the layout packs blocks contiguously).
// ---------------------------------------------------------------------------

void vfs_store_blocks_overlapping(
    const VfsPlan::VfsFile &file,
    uint64_t start,
    uint64_t end,
    std::vector<uint32_t> *out);

// materializes exactly one asset through the same write path as the batch
// driver (restricted to one item), assuming the blob is
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

// one-asset materialization from bytes the caller already holds (the trap's
// range fetch delivers the block straight from the source file): same write
// path, no store read. Range assets expect [ml metadata][payload].
bool vfs_store_materialize_asset_data(
    const VfsPlan &plan,
    const vfs_store_params &params,
    std::vector<uint8_t> &states,
    vfs_journal *journal,
    uint32_t file_index,
    uint32_t asset_index,
    const std::vector<uint8_t> &block_bytes);

// assembles the whole pack file for `file_index` in the install dir from the
// arena's granules (web: the game mmaps packs at init and reads blocks through
// baked pointers; the arena keeps granules in memory only, so the packs the
// game opens as files must exist on disk before init). Writes the exact native
// layout: synthesized [ml] metadata + inflated payload at the planned offsets
// plus the patched table entry (Flags 0). A pack whose on-disk size and
// fingerprint already match the plan is left untouched (warm boots). Every
// asset is ensured first (fetching from the store on demand); the arena budget
// holds - one granule is resident at a time, the rest streams into the file.
bool vfs_store_materialize_pack(
    class vfs_store_arena *arena,
    const VfsPlan &plan,
    size_t file_index,
    const fs_path &install_dir,
    const fs_path &blobs_dir);

// arena-free materialization result: how the pack's assets were written
struct vfs_pack_write_stats
{
    size_t written = 0; // real bytes landed in the pack file
    size_t stubbed = 0; // blob missing from the store: zero slot, no table entry
    size_t failed = 0;  // blob present but write/decompress failed
    size_t skipped = 0; // 1 when a matching file was already on disk
    uint64_t bytes = 0; // payload bytes written
};

// web: writes ONLY the pack's header + section + entry table (the block
// hashmap the game builds from it) - never the planned full size. The game's
// punctual fetch layer serves block bytes; the full pack exists nowhere.
bool vfs_store_materialize_skeleton(
    const VfsPlan &plan, size_t file_index, const fs_path &install_dir, uint64_t *out_size);

// arena-free counterpart of vfs_store_materialize_pack: payloads are read
// straight from the blob store (no granule residency), so it never touches
// the wasm heap beyond one asset at a time. Assets whose blob is missing keep
// their zeroed slot and are reported through the returned stats instead of
// aborting the pack (the caller decides whether the gate may open).
vfs_pack_write_stats vfs_store_materialize_pack_from_store(
    const VfsPlan &plan,
    size_t file_index,
    const fs_path &install_dir,
    const fs_path &blobs_dir);

// update flow: re-opens the hole of one pack asset - the
// on-disk entry keeps the plan's fields but Offset goes back to 0, so every
// presence check (reconcile, verify, trap) reports the block absent and the
// normal paths re-fetch/re-write it. Loose assets are invalidated by
// deleting the file (the skeleton re-sizes it). The caller resets the state
// byte and journals the reset; this only touches the materialized file.
bool vfs_store_invalidate_asset(
    const VfsPlan &plan,
    const vfs_store_params &params,
    uint32_t file_index,
    uint32_t asset_index);

// true when the filesystem holding `probe_path`'s directory grows files
// sparsely (a freshly truncated file occupies far fewer blocks than its
// size). Fat/exFAT report full allocation - the install then really
// preallocates every planned byte, which the store warns about once.
bool vfs_store_fs_supports_sparse(const fs_path &probe_dir);

// full verify pass over the plan; returns the number of corrupt assets (0
// when clean) and fills `out` (optional) with the per-asset details.
// `states` (optional, plan-ordered state DB bytes) makes the pass stub-aware:
// assets the DB marks vfs_stubbed report missing-block + the stubbed flag
// instead of corrupt (their on-disk entry is a placeholder patch)
int vfs_store_verify(
    const VfsPlan &plan,
    const vfs_store_params &params,
    vfs_verify_result *out,
    const std::vector<uint8_t> *states = nullptr);

// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_state: the persisted VFS state DB (VUOI plan §6.6), kept under
// <state_dir>:
//   plan.bin      - the serialized AssetPlan (deterministic bytes)
//   state.bin     - one state byte per plan asset, in plan order
//   journal.log   - append-only, fsync'd completion records for crash
//                   reconciliation
// No external dependencies; flat, debuggable files. states[i] indexes assets
// in plan order: files in VfsPlan::files order, assets in file order.

#pragma once

#include <stdint.h>
#include <stdio.h>
#include <vector>

#include <common/fs.h>

#include "vfs_manifest.h"

enum vfs_asset_state : uint8_t
{
    vfs_pending = 0, // not materialized yet (or unknown after a crash)
    vfs_fetched = 1, // bytes written to the output file and confirmed
    vfs_stubbed = 2, // reserved: placeholder served (M4)
    vfs_failed = 3,  // last attempt failed (corrupt blob, io error)
};

// ---------------------------------------------------------------------------
// plan.bin
// ---------------------------------------------------------------------------

bool vfs_plan_save(const fs_path &path, const VfsPlan &plan);
bool vfs_plan_load(const fs_path &path, VfsPlan &out_plan);

// canonical byte serialization of a plan; used by plan.bin and as the
// equality check for determinism tests
bool vfs_plan_serialize(const VfsPlan &plan, std::vector<uint8_t> &out);
bool vfs_plan_deserialize(const uint8_t *data, size_t size, VfsPlan &out_plan);

// ---------------------------------------------------------------------------
// state.bin
// ---------------------------------------------------------------------------

size_t vfs_state_asset_count(const VfsPlan &plan);

bool vfs_state_save(const fs_path &path, const std::vector<uint8_t> &states);
// flushes an already-written file to stable storage (the guarded fsync
// pattern used by the journal, for callers that hold no FILE* handle)
bool vfs_state_file_sync(const fs_path &path);
// a missing, truncated or oversized state.bin yields all-pending states
bool vfs_state_load(const fs_path &path, size_t asset_count, std::vector<uint8_t> &out_states);

// ---------------------------------------------------------------------------
// journal.log
// ---------------------------------------------------------------------------

struct vfs_journal_record
{
    char op = 0; // 'F': asset fetched (block written), 'S': file sealed
    uint32_t file_index = 0;
    uint32_t asset_index = 0;
};

struct vfs_journal
{
    FILE *handle = nullptr;
    fs_path path;
};

bool vfs_journal_open(vfs_journal &journal, const fs_path &path);
bool vfs_journal_close(vfs_journal &journal);
bool vfs_journal_mark(vfs_journal &journal, const vfs_journal_record &record);
// replays every record appended to the journal (or across truncation: partial
// trailing lines are ignored)
bool vfs_journal_replay(const fs_path &path, std::vector<vfs_journal_record> &out);

// applies fetched journal records on top of the given states; out of range
// records (stale or corrupted journal) are ignored
void vfs_state_apply_journal(
    const VfsPlan &plan,
    const std::vector<vfs_journal_record> &records,
    std::vector<uint8_t> &states);

// ---------------------------------------------------------------------------
// reconcile
// ---------------------------------------------------------------------------

struct vfs_reconcile_stats
{
    size_t assets_checked = 0;
    size_t assets_repaired = 0; // fetched -> pending downgrades
    size_t assets_upgraded = 0; // pending -> fetched confirmations
};

// Applies journal records on top of `states`, then spot checks every fetched
// asset against the on-disk file (size, entry table and checksum fields for
// packs; size for loose files), downgrading anything inconsistent to pending
// so a later materialize pass repairs it. The materialized files are the
// source of truth; the DB is rebuilt from them, never the other way around.
bool vfs_state_reconcile(
    const VfsPlan &plan,
    const fs_path &install_dir,
    std::vector<uint8_t> &states,
    vfs_reconcile_stats *stats);

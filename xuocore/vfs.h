// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs: public VUOI core API, M1 subset of plan §6.1 (plan/state/materialize/
// verify over a local blob store; boot, fetch, trap and stub APIs arrive in
// M2+). The install dir produced here is the future uo_data_init() target.

#pragma once

#include <atomic>
#include <common/fs.h>

#include "vfs_manifest.h"
#include "vfs_state.h"
#include "vfs_store.h"

struct VfsStats
{
    size_t files_total = 0;
    size_t files_done = 0;  // files with every expected asset present
    size_t bytes_total = 0; // planned bytes (pack tables + blocks, loose files)
    size_t bytes_done = 0;  // bytes confirmed in place
    bool online = false;    // fetch engine connected to a repo (M2)
};

struct vfs_params
{
    bool enabled = false; // streaming install mode (M2); false: no-op module
    bool offline = false; // never hit the network; cache/stubs only
    uint32_t workers = 0; // download workers; 0: default (cpu-1, cap 8)
    fs_path blobs_dir;   // local blob store root (mirrors the remote URL tree)
    fs_path install_dir; // materialized game files
    fs_path state_dir;   // state DB; default: <install_dir>/cache/state
    // product manifest; default: <blobs_dir>/uo-legacyrelease.prod, resolved
    // against the blobs root when the given value is not an existing path
    fs_path product_file;
    // where the product manifest is downloaded from when missing (M2);
    // empty: the official URL (VUOI plan §3.1)
    astr_t product_url;
    // §7.5 deployment recorder (--streaming-record); empty: disabled
    fs_path record_path;
    astr_t record_scenario; // --streaming-record-scenario (default "default")
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
// fatal (they are M2's fetch list)
bool vfs_materialize(vfs_materialize_stats *stats);

// verify materialized files against plan and blobs; per-file statuses in
// `out`; returns the number of corrupt assets (0 when clean, -1 when the
// plan cannot be built), which is what makes this a hard failure
int vfs_verify(vfs_verify_result *out);

// aggregate progress over the current plan and state DB
void vfs_stats(VfsStats *out);

// ---------------------------------------------------------------------------
// boot (M2, VUOI plan §7.2): plan -> blocking T0 fetch+materialize (with
// progress) -> background T1; returns false on fatal errors (offline mode
// with an insufficient store, unreachable repos, failed core assets)
// ---------------------------------------------------------------------------

bool vfs_boot(void (*progress)(float done01, const char *label));
// headless tools (vuoi): wait for the background tier to finish instead of
// leaving it running; the game never calls this (it must not block the loop)
void vfs_boot_wait();
// stops the fetch engine and the background tier, saves the state DB
void vfs_shutdown();

// ---------------------------------------------------------------------------
// managed mappings (M3, VUOI plan §6.1, §6.5): hole-paged mappings with
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

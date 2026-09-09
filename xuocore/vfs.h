// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs: public VUOI core API, M1 subset of plan §6.1 (plan/state/materialize/
// verify over a local blob store; boot, fetch, trap and stub APIs arrive in
// M2+). The install dir produced here is the future uo_data_init() target.

#pragma once

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
    bool online = false;    // M1: always false (offline blob store only)
};

struct vfs_params
{
    fs_path blobs_dir;   // local blob store root (mirrors the remote URL tree)
    fs_path install_dir; // materialized game files
    fs_path state_dir;   // state DB; default: <install_dir>/cache/state
    // product manifest; default: <blobs_dir>/uo-legacyrelease.prod, resolved
    // against the blobs root when the given value is not an existing path
    fs_path product_file;
};

void vfs_set_params(const vfs_params &params);
const vfs_params &vfs_get_params();
const char *vfs_install_path();

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

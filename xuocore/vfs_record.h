// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_record: the asset-access recorder. Every asset that transitions
// pending -> fetched is appended to a CSV:
//
//   hash,pack,ul,tier_hint,first_access_ms,scenario
//
// The recording seeds the embedded T1 manifest: a scripted first-run replayed
// under the recorder produces the common-asset order compiled into the client
// (see vfs_manifest_embedded.cpp). Enabled only when a record path is
// configured (--streaming-record); otherwise it compiles down to a flag check.

#pragma once

#include <stdint.h>

#include <common/fs.h>
#include <common/str.h>

#include "vfs_manifest.h"

// enables the recorder; opens (creates or truncates) the CSV right away and
// writes the header. When `path` is an existing directory, the CSV is
// recorded inside it as recording-<scenario>.csv
void vfs_record_configure(const fs_path &path, const char *scenario);
bool vfs_record_enabled();
// appends one record (thread-safe); milliseconds are counted since configure
void vfs_record_asset(uint64_t uop_hash, const astr_t &pack, uint64_t ul, uint8_t tier_hint);
// flushes and closes the CSV (called from vfs_shutdown)
void vfs_record_close();

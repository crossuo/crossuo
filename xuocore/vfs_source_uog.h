// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_source_uog: the "uog" streaming-install format (UOGamers' cdn). The
// product manifest is plain json - a single FileList array of EVERY file the
// install ships (launcher binaries, Sallos/, music, ...), one object per
// file, keys in any order:
//
//   {"FileName":"\\anim.idx","FileSize":1785720,
//    "LastWrite":"2025-10-28T20:41:01.8507375Z","MD5Hash":"yoqPkQQ6atGuo0UNJDjxTQ=="}
//
// FileNames are backslash-separated windows paths rooted at the install
// root; LastWrite is iso-8601; MD5Hash is the base64 of the file's md5. The
// plan keeps only the UO data files - the shared classifier over the stock
// client's file names (vfs_aops_filter_accept) - materialized lowercase;
// every entry is one whole-file blob stored on the cdn under its manifest
// name and streams by plain GET.

#pragma once

#include "vfs_manifest.h"

// format sniffing over product bytes: both manifest formats flow through
// the same product_file/product_url plumbing, the build dispatches on
// content
bool vfs_uog_sniff_manifest(const uint8_t *data, size_t size);

// the product serial: a 64-bit content hash of the manifest bytes (the
// plan's drift fingerprint, same value vfs_product_serial reports)
uint64_t vfs_uog_serial(const uint8_t *data, size_t size);

// builds the plan from the raw manifest bytes (the product file); blobs
// resolve against the cdn root product_url hangs from
bool vfs_uog_build_plan(
    const vfs_manifest_params &params,
    const std::vector<uint8_t> &data,
    const astr_t &product_url,
    VfsPlan &out_plan);

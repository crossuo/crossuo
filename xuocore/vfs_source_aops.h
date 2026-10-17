// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_source_aops: the "aops" patch-manifest format (AOS-patcher family,
// e.g. uniteuo-hosted shards). A patchcfg INI names the patch data
// directory, a custom file extension and a salt url; the patchlist itself
// is a UTF-16 pipe-separated table of EVERY file the shard ships
// (launcher binaries, web art, ...), one entry per line:
//
//   <relpath>|<DDMMYYYY>|<hh>|<mm>|<ss>|0|<size>|<flag>|<flag>|<hash>
//
// The hash is informational (it does not match the served bytes); the file
// size is the verification. The plan keeps only the UO data files - a
// compiled pattern table over the stock client's file names - materialized
// lowercase; every entry is one whole-file blob stored on the patch server
// as <relpath><extension>. Large .uop entries refine into per-block range
// assets after the build (vfs_manifest_refine_ranges; these servers serve
// HTTP Range), so even packs stream by page fault.

#pragma once

#include "vfs_manifest.h"

// the patch server configuration decoded from the patchcfg ini
struct vfs_aops_config
{
    astr_t custom_extension = "aops"; // server files carry it after the name
    astr_t patch_salt_url;            // launcher salt (informational here)
    astr_t patch_directory;           // the patch data url (file repo)
    astr_t patchlist_url;             // the patchlist url (manifest repo)
    astr_t version_file;              // the shard's version marker file name
};

// format sniffing over product bytes: both manifest formats flow through
// the same product_file/product_url plumbing, the build dispatches on
// content
bool vfs_aops_sniff_patchlist(const uint8_t *data, size_t size);
bool vfs_aops_sniff_patchcfg(const uint8_t *data, size_t size);

// the product manifest url for an aops-style product url: a bare root
// (trailing /, .../patchlist, .../patchcfg) normalizes onto .../patchlist;
// a full manifest file url (a dot after the last /: the mythic
// "uo-legacyrelease.prod", a "prod.xml", ...) passes through unchanged.
// Idempotent. The web flow still passes bare roots (shell.html routes aops
// servers through /stream/<root>/ and data mounts as /data/<mount>/, and
// xuoproxy serves both at <root>patchlist), so the fetch site normalizes.
astr_t vfs_aops_product_url_canonical(const astr_t &root_or_url);

// the product serial: a 64-bit content hash of the patchlist bytes (the
// plan's drift fingerprint, same value vfs_product_serial reports)
uint64_t vfs_aops_serial(const uint8_t *data, size_t size);

// the shared UO-data classifier used by both sources: accepts a lowercased
// basename when it names a file a client install is made of (data
// extensions, language stems, exact data-file names)
bool vfs_aops_filter_accept(const astr_t &basename_lower);

// parses the patchcfg ini: data urls resolve relative to `root`, the
// extension and the version-file name carry through to the plan build
bool vfs_aops_parse_patchcfg(
    const uint8_t *data, size_t size, const astr_t &root, vfs_aops_config *out);

// builds the plan from the raw patchlist bytes (the product file) plus the
// params' fetch seam (patchcfg acquisition) and blobs_dir (patchcfg cache).
// The patchcfg url derives from product_url's directory.
bool vfs_aops_build_plan(
    const vfs_manifest_params &params,
    const std::vector<uint8_t> &patchlist,
    const astr_t &product_url,
    VfsPlan &out_plan);

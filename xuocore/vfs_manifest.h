// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_manifest: consumes EA patch manifests (product -> pkg.mft -> per-pack
// manifests) through the mft engine and produces the persisted AssetPlan
// (VUOI plan §6.2). The plan is the single source of truth for the
// materializer: it fixes, before any byte is written, the set of output
// files, the pack entry tables and the deterministic planned offset of every
// UOP block.

#pragma once

#include <stdint.h>
#include <vector>

#include <common/fs.h>
#include <common/str.h>

// Same type as mft.h's mft_fetch_fn (identical alias redeclaration, legal
// C++). mft.h is not included here on purpose: it pulls tinyxml2.h, which
// must be included before common/logging.h, and most consumers include this
// header after it (this is the repo-wide include-order workaround).
using mft_fetch_fn = bool (*) (
    void *context,
    const char *url,
    const char *rel_path,
    const char *local_path,
    uint8_t *buffer,
    size_t *size);

// tier of service: when the asset is needed (plan §7.3). Plan-only in M1:
// every asset defaults to T2 (on-demand) until vfs_boot (M2) assigns tiers.
enum vfs_tier : uint8_t
{
    vfs_tier_t0 = 0, // boot-critical, blocking
    vfs_tier_t1,     // common assets, early background
    vfs_tier_t2,     // on demand
    vfs_tier_t3,     // rare / never on boot
};

struct VfsAsset
{
    uint64_t uop_hash = 0;  // (sh << 32) | ph; 0 for loose files
    astr_t uop_file;        // pack file name ("artLegacyMUL.uop"); empty for loose
    astr_t name;            // loose-file name ("tiledata.mul"); empty for pack parts
    size_t ul = 0;          // uncompressed (payload) length, manifest "ul"
    size_t cl = 0;          // compressed (as shipped in the store) length, "cl"
    uint32_t ct = 0;        // compression type: 0 stored, 1 zlib
    uint32_t ml = 0;        // metadata size (bytes preceding the payload), "ml"
    uint32_t mc = 0;        // metadata checksum, "mc" (written to UopFileEntry::Checksum)
    uint32_t rh = 0;        // provenance hash, "rh"
    uint64_t timestamp = 0; // manifest timestamp, "t"
    // planned byte offset of the block in the materialized pack; 0 for holes
    // (blockless entries), which stay unwritten and skipped by UopLoadFile
    uint64_t file_offset = 0;
    uint8_t tier = vfs_tier_t2;
    astr_t blob; // blob store path relative to the blobs root (URL path mirror)
};

struct VfsPlan
{
    struct VfsFile
    {
        astr_t name;       // output file name ("artLegacyMUL.uop", "tiledata.mul")
        uint64_t size = 0; // planned total size (packs: table + blocks; loose: ul)
        bool is_uop = false;
        std::vector<VfsAsset> assets;
    };
    std::vector<VfsFile> files;
    uint64_t product_serial = 0;
    // repo base URLs from the product manifest (M2): the fetch engine composes
    // download URLs as repo + the asset's blob tail
    astr_t manifest_repo;
    astr_t file_repo;
};

// bytes the asset occupies inside its pack: [ml metadata][payload]; zero
// marks a hole (reserved entry, nothing to fetch or write)
inline uint64_t vfs_asset_block_size(const VfsAsset &asset)
{
    return uint64_t(asset.ml) + (asset.ct == 0 ? asset.ul : asset.cl);
}

inline bool vfs_asset_is_hole(const VfsAsset &asset)
{
    return vfs_asset_block_size(asset) == 0;
}

inline bool vfs_asset_is_loose(const VfsAsset &asset)
{
    return asset.uop_file.empty();
}

struct vfs_manifest_params
{
    fs_path product_file; // product manifest ("prod" XML), read from disk
    fs_path blobs_dir;    // local blob store root, mirroring the remote URL tree
    // optional acquisition override (M2: the HTTP fetch engine); when null,
    // every manifest must already exist in the blob store (M1 offline path)
    mft_fetch_fn fetch = nullptr;
    void *fetch_context = nullptr;
};

// Guard for manifest-controlled relative paths ("n=", "rpath=", pack names)
// before they are joined under the blob store or install dir: rejects empty
// strings, absolute paths, "." / ".." (and empty) components, backslashes,
// drive-letter prefixes and control characters.
bool vfs_path_component_ok(const char *s);

// Consumes the whole manifest tree offline (every manifest must exist in the
// blob store) and computes the deterministic plan layout. Same inputs always
// produce identical plan bytes.
bool vfs_manifest_build(const vfs_manifest_params &params, VfsPlan &out_plan);

// ---------------------------------------------------------------------------
// tiers (VUOI plan §7.3): assigned at boot from static name patterns plus the
// embedded T1 order (§7.5); the persisted plan stays tier-free (M1 bytes)
// ---------------------------------------------------------------------------

// one entry of the embedded (recorded) T1 manifest
struct vfs_embedded_entry
{
    uint64_t uop_hash = 0; // (sh << 32) | ph; 0 for loose files
    astr_t uop_file;       // pack name ("" for loose-file entries)
    astr_t name;           // loose-file name ("" for pack entries)
};

// The embedded T1 manifest generated from a recorded run (§7.5). Lives in
// vfs_manifest_embedded.cpp, which is the file the build step rewrites; the
// checked-in stub ships an empty table (T1 then only holds the static set).
const std::vector<vfs_embedded_entry> &vfs_manifest_embedded();

// applies the §7.3 tier table + the embedded T1 order to a freshly built plan
void vfs_manifest_assign_tiers(VfsPlan &plan);

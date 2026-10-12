// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_manifest: consumes EA patch manifests (product -> pkg.mft -> per-pack
// manifests) through the mft engine and produces the persisted AssetPlan.
// The plan is the single source of truth for the
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

// tier of service: when the asset is needed. Assets default to T2
// (on-demand) until vfs_boot assigns tiers.
enum vfs_tier : uint8_t
{
    vfs_tier_t0 = 0, // boot-critical, blocking
    vfs_tier_t1,     // common assets, early background
    vfs_tier_t2,     // on demand
    vfs_tier_t3,     // rare / never on boot
};

// product subset: a per-build filter over the SAME manifests
// so a web plan is a strict subset of the native one - one manifest tree, one
// granule model, no forked products
enum vfs_subset : uint8_t
{
    vfs_subset_none = 0, // the full product view (native default)
    vfs_subset_web,      // boot drops the legacy anim/music/facet/verdata
                         // bytes (loose files stay as streamable metadata)
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
    // range assets: wire offset of the block ([ml metadata][payload]) within
    // the source blob - the block streams in by HTTP Range instead of
    // downloading the whole blob. 0 = the blob is the whole payload (and 0 is
    // unambiguous: a real block never starts inside a valid archive's header)
    uint64_t blob_offset = 0;
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
        // subset-retained metadata: the file is outside this build's boot
        // view (e.g. the web subset's legacy anim mul/music/facet/verdata
        // loose files) but the plan keeps its entry (name, sizes, blob tail)
        // so an on-demand lane (stream files) can still acquire the bytes.
        // Every byte-touching path (loose fetch tier, skeleton/prepare,
        // materialize, refine, managed mapping, whole-blob passes) must skip
        // these files: metadata is KB, bytes only on demand.
        bool streamable = false;
        std::vector<VfsAsset> assets;
    };
    std::vector<VfsFile> files;
    uint64_t product_serial = 0;
    // repo base URLs from the product manifest: the fetch engine composes
    // download URLs as repo + the asset's blob tail
    astr_t manifest_repo;
    astr_t file_repo;
    // product subset the plan was filtered to; persisted in
    // plan.bin (v4). Missing (older plans) means vfs_subset_none
    uint8_t subset = vfs_subset_none;
};

// bytes the asset occupies inside its pack: [ml metadata][stored payload].
// Payloads materialize decompressed (entry Flags == 0, CompressedSize == ul)
// so the raw-pointer readers (art, map blocks) see stored bytes exactly like
// EA's own local archives; the compressed blob (cl) only ever exists in the
// blob store and inflates right before the block write. Zero marks a hole
// (reserved entry, nothing to fetch or write)
inline uint64_t vfs_asset_block_size(const VfsAsset &asset)
{
    return uint64_t(asset.ml) + asset.ul;
}

// bytes to transfer for one asset: range assets stream their block
// ([ml metadata][payload]) from the source blob; whole-blob assets transfer
// the payload as shipped
inline uint64_t vfs_asset_range_len(const VfsAsset &asset)
{
    const uint64_t payload = asset.ct == 0 ? asset.ul : asset.cl;
    return asset.blob_offset != 0 ? uint64_t(asset.ml) + payload : payload;
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
    fs_path product_file; // product manifest (MythicMFT XML, aops patchlist, uog json)
    // the product's remote url when known: the aops source derives its
    // patchcfg url (config beside the patchlist) from it, the uog source
    // its blob repo (the manifest url's directory)
    astr_t product_url;
    fs_path blobs_dir;    // local blob store root, mirroring the remote URL tree
    // optional acquisition override (the HTTP fetch engine); when null,
    // every manifest must already exist in the blob store (offline path)
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

// plan refinement: whole-file range assets that are UOP archives become
// per-block range packs - every archive entry becomes its own asset backed by
// a byte range of the original file, so a fault streams one block (HTTP
// Range) instead of the whole file. The table is walked once here (local blob
// store when the file is cached, ranged GETs against file_repo otherwise);
// files that cannot be walked stay loose. `min_size` skips small files.
bool vfs_manifest_refine_ranges(
    VfsPlan &plan, const fs_path &blobs_dir, const astr_t &file_repo, uint64_t min_size);

// ---------------------------------------------------------------------------
// update flow: compares two plans built from different
// product serials and reports what changed per asset. Identities are the
// uop hash (packs) and the file name (loose); a change is any difference in
// timestamp, provenance (rh), sizes or metadata - the same fields
// xuoi_listing diffs, so both tools agree on what re-downloads.
// ---------------------------------------------------------------------------

// a (file, asset) position inside one plan
struct vfs_plan_index
{
    uint32_t file_index = 0;
    uint32_t asset_index = 0;
};

struct vfs_plan_diff_result
{
    // assets present in both plans whose content identity changed: their
    // state resets to pending, the cached blob is dropped and the pack block
    // is invalidated (the hole re-opens) so the normal paths heal them
    std::vector<vfs_plan_index> changed; // indexes into the NEW plan
    // assets that only exist in the new plan (new: already pending, nothing
    // to reset; kept for reporting)
    std::vector<vfs_plan_index> added; // indexes into the NEW plan
    size_t removed = 0;                // old-plan assets the new plan dropped
};

// false when the plans cannot be compared (never expected: both are built
// plans); an empty result means the serials differ but nothing else does
bool vfs_plan_diff(
    const VfsPlan &old_plan,
    const VfsPlan &new_plan,
    vfs_plan_diff_result *out);

// ---------------------------------------------------------------------------
// tiers: assigned at boot from static name patterns plus the
// embedded T1 order; the persisted plan stays tier-free
// ---------------------------------------------------------------------------

// one entry of the embedded (recorded) T1 manifest
struct vfs_embedded_entry
{
    uint64_t uop_hash = 0; // (sh << 32) | ph; 0 for loose files
    astr_t uop_file;       // pack name ("" for loose-file entries)
    astr_t name;           // loose-file name ("" for pack entries)
};

// The embedded T1 manifest generated from a recorded run (boot accesses of a
// scripted first-run scenario). Lives in
// vfs_manifest_embedded.cpp, which is the file the build step rewrites; the
// checked-in stub ships an empty table (T1 then only holds the static set).
const std::vector<vfs_embedded_entry> &vfs_manifest_embedded();

// test/deployment seam: the table consumers actually use - the override
// above when one was injected, the embedded file's table otherwise. Tests
// inject converted recordings here (the deployment rewrite of
// vfs_manifest_embedded.cpp keeps this seam intact).
const std::vector<vfs_embedded_entry> &vfs_manifest_embedded_table();
// replaces the override table (empty restores the embedded file's table)
void vfs_manifest_embedded_set(std::vector<vfs_embedded_entry> table);

// the tier of one asset: the name tables key on the file and apply to both
// shapes (loose assets and refined pack blocks); pure - boot assigns tiers
// on the in-memory plan
uint8_t vfs_manifest_tier_of(const VfsPlan::VfsFile &file, const VfsAsset &asset);

// applies the tier table + the embedded T1 order to a freshly built plan
void vfs_manifest_assign_tiers(VfsPlan &plan);

// ---------------------------------------------------------------------------
// product subsets: a per-build filter over the SAME manifests
// so a web plan is a strict subset of the native one - one manifest tree,
// one granule model, no forked products (the vfs_subset enum is at the top).
// ---------------------------------------------------------------------------

// the web exclusion rule: true when the plan file `name` is outside the web
// boot view - the legacy MUL animation set ("anim*.mul": the bytes stream on
// demand through the stream-file lane), Music/** (deferred), facet* and
// verdata*. The matching LOOSE files stay in the plan as streamable
// metadata (see VfsFile::streamable); only UOP packs are dropped entirely.
bool vfs_subset_web_excludes(const astr_t &name);

// applies the subset filter in place (no-op for vfs_subset_none): excluded
// loose files are KEPT as streamable metadata (VfsFile::streamable) - the
// boot plan must never fetch or write their bytes, an on-demand lane may -
// and excluded UOP packs drop out entirely. Idempotent (re-applying the same
// subset changes nothing). Entries removed by the subset behave like
// plan-removed in the update flow: bytes stay in the store, accounting drops
// them (plan diff "removed").
void vfs_manifest_apply_subset(VfsPlan &plan, vfs_subset subset);

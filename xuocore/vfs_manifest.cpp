// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

#define LOGGER_MODULE Vfs

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <vector>

#include "mft.h"
#include "vfs_manifest.h"
#include "uop.h"

#include "common.h"
#include <common/logging/logging.h>
#include <common/utils.h>

#include <external/zlib_amalg.h>

// manifest blobs are small; the mft engine sizes its buffers from this
#define VFS_MANIFEST_BUFFER_SIZE (16 * 1024 * 1024)

// Blob-store acquisition for the mft fetch seam (M1): the store mirrors the
// remote URL tree 1:1, so the resource path is used as-is. No network is
// ever touched here; cache files are not consulted (the store is the truth).
static bool vfs_fetch_blob(
    void *context,
    const char *url,
    const char *rel_path,
    const char *local_path,
    uint8_t *buffer,
    size_t *size)
{
    (void)url;
    (void)local_path;
    const auto *blobs = static_cast<const fs_path *>(context);
    const auto path = fs_path_join(*blobs, rel_path);
    if (fs_file_read(fs_path_ascii(path), buffer, size))
    {
        LOG_TRACE("blob: %s (%zu bytes)", rel_path, *size);
        return true;
    }
    LOG_ERROR("blob not found in store: %s", rel_path);
    return false;
}

// Manifest-controlled strings are joined under the blob store and install
// dir with plain concatenation, so every component must be a plain name:
// anything that could climb out of the root ("..", absolute, drive letters)
// or smuggle separators (backslashes) is rejected up front.
bool vfs_path_component_ok(const char *s)
{
    if (!s || !*s || *s == '/' || *s == '\\')
        return false;
    for (const char *p = s; *p; ++p)
    {
        const auto c = uint8_t(*p);
        if (c < 0x20 || c == 0x7f || c == uint8_t('\\') || c == uint8_t(':'))
            return false; // control chars, backslashes and drive prefixes
    }
    const char *component = s;
    while (true)
    {
        const char *slash = strchr(component, '/');
        const size_t len = slash ? size_t(slash - component) : strlen(component);
        // empty (duplicate slashes), "." and ".." components escape the root
        if (len == 0 || (len == 1 && component[0] == '.') ||
            (len == 2 && component[0] == '.' && component[1] == '.'))
            return false;
        if (!slash)
            return true;
        component = slash + 1;
    }
}

static void vfs_plan_asset_load(const mft_entry &entry, VfsAsset &asset)
{
    asset.ul = entry.uncompressed_len;
    asset.cl = entry.compressed_len;
    asset.ct = entry.compression_type;
    asset.ml = entry.meta_len;
    asset.mc = entry.meta_crc;
    asset.rh = entry.hash;
    asset.timestamp = entry.timestamp;
}

// pack parts come from the per-pack manifests ("<packs><p ...><files><f .../>");
// parts of the same pack accumulate into one VfsFile, in first-appearance
// (manifest consumption) order, which fixes the entry table layout
static void vfs_plan_build_packs(mft_product &prod, VfsPlan &plan)
{
    std::map<astr_t, size_t> pack_index;
    char rpath[512] = {};
    for (auto &part : prod.parts)
    {
        if (!part.pack_name || !part.remote_path)
        {
            LOG_WARN("skipping pack part without pack name or remote path");
            continue;
        }
        // the pack name names the output file and the rpath composes the blob
        // tail: both are manifest-controlled, so both must be safe joins
        if (!vfs_path_component_ok(part.pack_name))
        {
            LOG_WARN("skipping pack part with unsafe pack rpath=\"%s\"", part.pack_name);
            continue;
        }
        if (!vfs_path_component_ok(part.remote_path))
        {
            LOG_WARN("skipping pack part with unsafe rpath=\"%s\"", part.remote_path);
            continue;
        }
        size_t file_index = 0;
        const astr_t pack_name = part.pack_name;
        const auto found = pack_index.find(pack_name);
        if (found != pack_index.end())
        {
            file_index = found->second;
        }
        else
        {
            VfsPlan::VfsFile file;
            file.name = pack_name;
            file.is_uop = true;
            plan.files.emplace_back(file);
            file_index = plan.files.size() - 1;
            pack_index[pack_name] = file_index;
        }

        VfsAsset asset;
        vfs_plan_asset_load(part, asset);
        asset.uop_hash = UOP_HASH(part.sh, part.ph);
        asset.uop_file = pack_name;
        mft_entry_remote_path(part, rpath);
        asset.blob = astr_t(rpath);
        plan.files[file_index].assets.emplace_back(asset);
    }
}

// loose files come from the "<files>" sections; each becomes its own VfsFile,
// materialized whole from its (decompressed) blob
static void vfs_plan_build_files(mft_product &prod, VfsPlan &plan)
{
    char rpath[512] = {};
    for (auto &file_entry : prod.files)
    {
        if (!file_entry.name || !file_entry.remote_path)
        {
            LOG_WARN("skipping file without name or remote path");
            continue;
        }
        // "n" names the output file and "rpath" composes the blob tail: both
        // are manifest-controlled, so both must be safe joins
        if (!vfs_path_component_ok(file_entry.name))
        {
            LOG_WARN("skipping file with unsafe n=\"%s\"", file_entry.name);
            continue;
        }
        if (!vfs_path_component_ok(file_entry.remote_path))
        {
            LOG_WARN("skipping file with unsafe rpath=\"%s\"", file_entry.remote_path);
            continue;
        }
        VfsPlan::VfsFile file;
        file.name = file_entry.name;
        file.is_uop = false;
        VfsAsset asset;
        vfs_plan_asset_load(file_entry, asset);
        asset.name = file_entry.name;
        mft_entry_remote_path(file_entry, rpath); // also fills ph/sh from the name
        asset.blob = astr_t(rpath);
        file.assets.emplace_back(asset);
        file.size = file_entry.uncompressed_len;
        plan.files.emplace_back(file);
    }
}

// deterministic layout: planned offsets depend only on manifest order and
// sizes. One section chain (capacity == entry count, as UopLoadFile expects),
// blocks packed in manifest order after the entry table. Holes (zero-size
// blocks) reserve their table slot but no bytes: their planned offset stays 0
// and the on-disk entry keeps Offset == 0 until (if ever) written.
static void vfs_plan_layout(VfsPlan &plan)
{
    for (auto &file : plan.files)
    {
        if (!file.is_uop)
        {
            continue;
        }
        uint64_t offset =
            sizeof(UopHeader) + sizeof(UopSection) + sizeof(UopFileEntry) * file.assets.size();
        for (auto &asset : file.assets)
        {
            const uint64_t block = vfs_asset_block_size(asset);
            if (block == 0)
            {
                asset.file_offset = 0;
                continue;
            }
            asset.file_offset = offset;
            offset += block;
        }
        file.size = offset;
    }

    // packs without entries produce no file at all
    for (size_t i = plan.files.size(); i > 0; --i)
    {
        const size_t index = i - 1;
        if (plan.files[index].is_uop && plan.files[index].assets.empty())
        {
            plan.files.erase(plan.files.begin() + ptrdiff_t(index));
        }
    }
}

bool vfs_manifest_build(const vfs_manifest_params &params, VfsPlan &out_plan)
{
    out_plan = VfsPlan();

    vfs_manifest_params p = params; // the fetch context outlives this call
    mft_config cfg;
    cfg.agent_name = "vuoi";
    cfg.download_buffer_size = VFS_MANIFEST_BUFFER_SIZE;
    cfg.dump_extracted = false;
    cfg.fetch = vfs_fetch_blob;
    cfg.fetch_context = &p.blobs_dir;

    std::vector<uint8_t> data;
    if (!fs_file_read(fs_path_ascii(params.product_file), data))
    {
        LOG_ERROR("could not read product manifest: %s", fs_path_ascii(params.product_file));
        return false;
    }

    mft_product prod;
    mft_init(prod, cfg);
    mft_result res = mft_load(prod, data.data(), data.size());
    if (res == mft_ok)
    {
        res = mft_consume_manifests(prod);
    }
    if (res == mft_ok)
    {
        out_plan.product_serial = prod.timestamp;
        vfs_plan_build_packs(prod, out_plan);
        vfs_plan_build_files(prod, out_plan);
        vfs_plan_layout(out_plan);
        LOG_INFO(
            "plan: %zu files, serial %" PRIu64, out_plan.files.size(), out_plan.product_serial);
    }
    mft_cleanup(prod);
    return res == mft_ok;
}

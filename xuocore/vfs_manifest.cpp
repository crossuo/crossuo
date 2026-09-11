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
#include "vfs_transport.h"
#include "uop.h"

#include <algorithm>

#include "common.h"
#include <common/fs.h>
#include <common/logging/logging.h>
#include <common/utils.h>

#include <external/zlib_amalg.h>

#define UOI_AGENT_NAME "EAMythic Patch Client"
// manifest blobs are small; the mft engine sizes its buffers from this
#define VFS_MANIFEST_BUFFER_SIZE (16 * 1024 * 1024)

// Blob-store acquisition for the mft fetch seam (offline path): the store
// mirrors the
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

// deterministic layout for one pack: planned offsets depend only on asset
// order and sizes. One section chain (capacity == entry count, as UopLoadFile
// expects), blocks packed in manifest order after the entry table. Holes
// (zero-size blocks) reserve their table slot but no bytes: their planned
// offset stays 0 and the on-disk entry keeps Offset == 0 until (if ever)
// written.
static void vfs_plan_layout_file(VfsPlan::VfsFile &file)
{
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

static void vfs_plan_layout(VfsPlan &plan)
{
    for (auto &file : plan.files)
    {
        if (!file.is_uop)
        {
            continue;
        }
        vfs_plan_layout_file(file);
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
    cfg.agent_name = UOI_AGENT_NAME;
    cfg.download_buffer_size = VFS_MANIFEST_BUFFER_SIZE;
    cfg.dump_extracted = false;
    cfg.fetch = p.fetch ? p.fetch : vfs_fetch_blob;
    cfg.fetch_context = p.fetch ? p.fetch_context : &p.blobs_dir;

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
        // the repos are the fetch engine's URL bases; empty means the
        // product manifest carried none (nothing can be downloaded)
        out_plan.manifest_repo = prod.manifest_repo ? prod.manifest_repo : "";
        out_plan.file_repo = prod.file_repo ? prod.file_repo : "";
        vfs_plan_build_packs(prod, out_plan);
        vfs_plan_build_files(prod, out_plan);
        vfs_plan_layout(out_plan);
        LOG_INFO(
            "plan: %zu files, serial %" PRIu64, out_plan.files.size(), out_plan.product_serial);
    }
    mft_cleanup(prod);
    return res == mft_ok;
}

// ---------------------------------------------------------------------------
// plan diff
// ---------------------------------------------------------------------------

// content identity of one asset: everything the fetch/materialize pipeline
// consumes. Same identity => same blob tail, same planned bytes, no work.
static bool vfs_plan_asset_same(const VfsAsset &a, const VfsAsset &b)
{
    return a.ul == b.ul && a.cl == b.cl && a.ct == b.ct && a.ml == b.ml && a.mc == b.mc &&
           a.rh == b.rh && a.timestamp == b.timestamp;
}

// pack assets are identified by their uop hash inside their pack; loose files
// by their output name. The uop hash alone can collide across packs (art
// legacy land tiles), so the pack name prefixes the key.
static astr_t vfs_plan_asset_key(const VfsAsset &asset)
{
    char suffix[24];
    snprintf(suffix, sizeof(suffix), "/%016" PRIx64, asset.uop_hash);
    if (vfs_asset_is_loose(asset))
        return astr_t("loose/") + asset.name;
    return astr_t("pack/") + asset.uop_file + suffix;
}

bool vfs_plan_diff(
    const VfsPlan &old_plan, const VfsPlan &new_plan, vfs_plan_diff_result *out)
{
    if (out == nullptr)
        return false;
    *out = vfs_plan_diff_result();

    std::map<astr_t, vfs_plan_index> old_index;
    for (size_t fi = 0; fi < old_plan.files.size(); ++fi)
    {
        const auto &file = old_plan.files[fi];
        for (size_t ai = 0; ai < file.assets.size(); ++ai)
            old_index[vfs_plan_asset_key(file.assets[ai])] = { uint32_t(fi), uint32_t(ai) };
    }

    std::map<astr_t, bool> new_ids;
    for (size_t fi = 0; fi < new_plan.files.size(); ++fi)
    {
        const auto &file = new_plan.files[fi];
        for (size_t ai = 0; ai < file.assets.size(); ++ai)
        {
            const auto &asset = file.assets[ai];
            const astr_t key = vfs_plan_asset_key(asset);
            new_ids[key] = true;
            const auto found = old_index.find(key);
            if (found == old_index.end())
                out->added.push_back({ uint32_t(fi), uint32_t(ai) });
            else if (!vfs_plan_asset_same(old_plan.files[found->second.file_index]
                                              .assets[found->second.asset_index],
                                          asset))
                out->changed.push_back({ uint32_t(fi), uint32_t(ai) });
        }
    }

    for (const auto &file : old_plan.files)
    {
        for (const auto &asset : file.assets)
        {
            if (new_ids.find(vfs_plan_asset_key(asset)) == new_ids.end())
                ++out->removed;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// tiers
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// embedded T1 manifest seam: consumers read through the override (set
// by tests / future resource loading) or the embedded file's compiled table
// ---------------------------------------------------------------------------

static std::vector<vfs_embedded_entry> s_embedded_override;

const std::vector<vfs_embedded_entry> &vfs_manifest_embedded_table()
{
    static const std::vector<vfs_embedded_entry> s_empty;
    return s_embedded_override.empty() ? vfs_manifest_embedded() : s_embedded_override;
}

void vfs_manifest_embedded_set(std::vector<vfs_embedded_entry> table)
{
    s_embedded_override = std::move(table);
}

// boot-critical loose files: never stubbed, fetched before Load() returns
static const char *const VFS_TIER_T0_FILES[] = {
    "tiledata.mul", "animdata.mul", "hues.mul",    "radarcol.mul", "skills.idx",
    "skills.mul",   "speech.mul",   "langcode.iff", "multimap.rle", "texidx.mul",
    "lightidx.mul", "light.mul",    "unifont.mul",  // + art/gump/sound/multi indexes
    "artidx.mul",   "gumpidx.mul",  "soundidx.mul", "multi.idx",    "multi.mul",
};

static bool vfs_tier_name_is(const astr_t &name, const char *const *table, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (strcasecmp(name.c_str(), table[i]) == 0)
            return true;
    }
    return false;
}

// T1 loose files: everything Load() itself reads before the first frame that
// is worth overlapping with it - the map-0 index (block table), anim.idx
// (LoadCommon requires it) and the first fonts. soundLegacyMUL rides along so
// its 133MB download overlaps Load instead of stalling it at the sound index.
// The rest of the map set (statics/facet/texmaps, difs) streams after the
// first frame (convergence materializes it, traps fetch it on first use).
static bool vfs_tier_t1_file(const astr_t &lower)
{
    static const char *const names[] = {
        "staidx0.mul",
        "anim.idx",
        "soundlegacymul.uop",
    };
    if (vfs_tier_name_is(lower, names, sizeof(names) / sizeof(names[0])))
        return true;
    for (int i = 1; i <= 6; ++i)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "unifont%d.mul", i);
        if (lower == buf)
            return true;
    }
    return false;
}

// MainMisc.uop core assets (boot-critical), keyed by their UOP name hashes
static const uint64_t VFS_TIER_T0_MAINMISC[] = {
    Asset::Strings,
    Asset::CommandsTranslate,
    Asset::TileAnimData,
    Asset::WearableLayers,
    Asset::AssetMap,
};

uint8_t vfs_manifest_tier_of(const VfsPlan::VfsFile &file, const VfsAsset &asset)
{
    if (vfs_asset_is_loose(asset))
    {
        const astr_t lower = str_lower(asset.name);
        if (lower.rfind("music/", 0) == 0)
            return vfs_tier_t3; // deferred to idle bandwidth
        if (vfs_tier_name_is(lower, VFS_TIER_T0_FILES, sizeof(VFS_TIER_T0_FILES) / sizeof(*VFS_TIER_T0_FILES)))
            return vfs_tier_t0;
        if (vfs_tier_t1_file(lower))
            return vfs_tier_t1;
        // anim.mul (190 MB legacy fallback) deliberately stays T2: the boot
        // skeleton prepares it valid-but-unfetched, LoadCommon passes, and the
        // idle tier converges the content
        return vfs_tier_t2;
    }

    // pack assets: only the MainMisc core set is T0; the rest comes from the
    // embedded manifest (T1) or defaults to on-demand (T2)
    if (strcasecmp(file.name.c_str(), "MainMisc.uop") == 0)
    {
        for (uint64_t hash : VFS_TIER_T0_MAINMISC)
        {
            if (asset.uop_hash == hash)
                return vfs_tier_t0;
        }
    }
    return vfs_tier_t2;
}

void vfs_manifest_assign_tiers(VfsPlan &plan)
{
    for (auto &file : plan.files)
    {
        for (auto &asset : file.assets)
        {
            asset.tier = vfs_manifest_tier_of(file, asset);
        }
    }

    // the embedded (recorded) manifest promotes its entries to T1; unknown
    // assets keep their static tier, so stale recordings only reorder
    const auto &embedded = vfs_manifest_embedded_table();
    size_t unmatched = 0;
    for (const auto &entry : embedded)
    {
        bool found = false;
        for (auto &file : plan.files)
        {
            if (entry.uop_file.empty() || file.name != entry.uop_file)
                continue;
            for (auto &asset : file.assets)
            {
                if (asset.uop_hash == entry.uop_hash)
                {
                    asset.tier = vfs_tier_t1;
                    found = true;
                    break;
                }
            }
            if (found)
                break;
        }
        if (!found)
            ++unmatched; // stale recording: pack/asset no longer in the plan
    }
    LOG_INFO("tiers: %zu embedded T1 entries", embedded.size());
    if (unmatched != 0)
    {
        LOG_WARN(
            "tiers: %zu embedded entries not in the plan (stale recording, advisory only)",
            unmatched);
    }
}

// ---------------------------------------------------------------------------
// product subsets: one manifest tree, one granule model, a
// per-build view over it. The web plan simply excludes what the browser
// build does not carry (see vfs_manifest.h for the pattern list).
// ---------------------------------------------------------------------------

bool vfs_subset_web_excludes(const astr_t &name)
{
    if (name.empty())
        return false;
    const astr_t lower = str_lower(name);
    const auto starts = [&lower](const char *prefix)
    {
        return lower.rfind(prefix, 0) == 0;
    };
    // anim*.mul: the legacy MUL animation set (560 MB - the UOP animations
    // cover it); anim.idx stays: it is KB-sized and not worth a special case
    if (starts("anim") && lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".mul") == 0)
        return true;
    // Music/**: deferred on the web build
    if (starts("music/"))
        return true;
    // facet*: the facet maps are not part of the web view
    if (starts("facet"))
        return true;
    // verdata*: the legacy patch file (the UOP data supersedes it)
    if (starts("verdata"))
        return true;
    return false;
}

void vfs_manifest_apply_subset(VfsPlan &plan, vfs_subset subset)
{
    if (subset == vfs_subset_none)
        return;
    plan.subset = uint8_t(subset);
    size_t removed = 0;
    for (size_t i = plan.files.size(); i > 0; --i)
    {
        const size_t index = i - 1;
        if (!vfs_subset_web_excludes(plan.files[index].name))
            continue;
        plan.files.erase(plan.files.begin() + ptrdiff_t(index));
        ++removed;
    }
    LOG_INFO("subset: web view keeps %zu of %zu files", plan.files.size(), plan.files.size() + removed);
}

// ---------------------------------------------------------------------------
// plan refinement: whole-file range assets that are UOP archives become
// per-block range packs. The original file keeps its bytes exactly; the plan
// just stops treating it as one indivisible blob. A fault then streams one
// block ([ml metadata][payload]) by HTTP Range instead of the whole file -
// the difference between 264MB and ~10KB on the boot path.
// ---------------------------------------------------------------------------

namespace
{
// span-cached reader over the original file: local blob store when cached,
// ranged GETs against the file repo otherwise. `view` keeps the last window,
// so sequential chain walks that stay inside one window cost a single
// transfer (contiguous section chains collapse into few requests).
struct vfs_refine_reader
{
    FILE *local_fp = nullptr;
    astr_t url; // absolute URL of the source file (repo + blob tail)
    astr_t tail;
    std::vector<uint8_t> span;
    uint64_t span_off = 0;
    uint64_t source_size = 0;
    bool failed = false;
    size_t transfers = 0;

    static const size_t CHUNK = 256 * 1024;

    static bool span_sink(void *context, const uint8_t *data, size_t size)
    {
        static_cast<std::vector<uint8_t> *>(context)->assign(data, data + size);
        return true;
    }

    ~vfs_refine_reader()
    {
        if (local_fp)
            fclose(local_fp);
    }

    const uint8_t *view(uint64_t offset, size_t length)
    {
        if (failed)
            return nullptr;
        if (offset >= span_off && offset + length <= span_off + span.size())
            return span.data() + size_t(offset - span_off);
        const uint64_t want = std::min<uint64_t>(
            std::max<uint64_t>(length, CHUNK), source_size - offset);
        span.assign(size_t(want), 0);
        span_off = offset;
        if (local_fp != nullptr)
        {
            if (fseeko(local_fp, off_t(offset), SEEK_SET) != 0 ||
                fread(span.data(), 1, span.size(), local_fp) != span.size())
            {
                failed = true;
                return nullptr;
            }
        }
        else
        {
            vfs_transport_result res = vfs_transport_active()->range(
                url.c_str(), tail.c_str(), offset, span.size(), &vfs_refine_reader::span_sink,
                &span, 0, false);
            if (!res.ok || res.bytes != span.size())
            {
                failed = true;
                return nullptr;
            }
        }
        ++transfers;
        return span.data();
    }
};

bool vfs_refine_walk_table(vfs_refine_reader &reader, std::vector<UopFileEntry> &entries)
{
    // header: [0, sizeof(UopHeader))
    const uint8_t *raw = reader.view(0, sizeof(UopHeader));
    if (raw == nullptr)
        return false;
    UopHeader header;
    memcpy(&header, raw, sizeof(header));
    if (header.Magic != MYP_MAGIC || header.Version < 4 || header.Version > 5)
        return false;
    uint64_t cur = header.SectionOffset;
    size_t guard = 0;
    while (cur != 0)
    {
        if (++guard > 100000)
            return false;
        // section: FileCount + NextSection, then its entry table
        const uint8_t *sec = reader.view(cur, sizeof(UopSection));
        if (sec == nullptr)
            return false;
        UopSection section;
        memcpy(&section, sec, sizeof(section));
        if (section.FileCount == 0)
            return false; // a chain never carries empty sections
        const uint8_t *table =
            reader.view(cur + sizeof(UopSection), size_t(section.FileCount) * sizeof(UopFileEntry));
        if (table == nullptr)
            return false;
        for (uint32_t i = 0; i < section.FileCount; ++i)
        {
            UopFileEntry entry;
            memcpy(&entry, table + size_t(i) * sizeof(entry), sizeof(entry));
            if (entry.Offset == 0)
                continue; // hole: reserved slot, no bytes anywhere
            if (uint64_t(entry.MetadataSize) + entry.DecompressedSize > reader.source_size ||
                entry.Offset + entry.MetadataSize + entry.CompressedSize > reader.source_size)
                return false; // entry points outside the file: not a table we trust
            // only encodings the materializer round-trips: stored (0) and
            // zlib (1). BWT blocks (3, e.g. gumpart) keep their file whole
            // until the writer learns to relocate them
            if (entry.Flags != 0 && entry.Flags != 1)
                return false;
            entries.push_back(entry);
        }
        cur = section.NextSection;
    }
    return !entries.empty();
}

bool vfs_refine_file(VfsPlan::VfsFile &file, const fs_path &blobs_dir, const astr_t &file_repo)
{
    const auto &loose = file.assets.front();
    const auto blob_path = fs_path_join(blobs_dir, loose.blob);

    vfs_refine_reader reader;
    reader.source_size = file.size;
    reader.tail = loose.blob;
    if (fs_path_exists(blob_path))
    {
        reader.local_fp = fopen(fs_path_ascii(blob_path), "rb");
        if (reader.local_fp == nullptr)
        {
            LOG_WARN("refine: %s could not open the cached blob", file.name.c_str());
            return false;
        }
    }
    else
    {
        if (file_repo.empty())
        {
            LOG_WARN("refine: %s is not cached and no file repo is set", file.name.c_str());
            return false;
        }
        reader.url = file_repo + loose.blob; // repo + tail, the fetch engine's join
    }

    std::vector<UopFileEntry> entries;
    entries.reserve(1024);
    if (!vfs_refine_walk_table(reader, entries))
    {
        LOG_WARN(
            "refine: could not walk %s (%zu transfers), keeping it whole", file.name.c_str(), reader.transfers);
        return false;
    }

    VfsPlan::VfsFile refined;
    refined.name = file.name;
    refined.is_uop = true;
    refined.assets.reserve(entries.size());
    for (const auto &entry : entries)
    {
        VfsAsset asset;
        asset.uop_hash = entry.Hash;
        asset.uop_file = file.name;
        asset.ul = entry.DecompressedSize;
        // the source block's payload encoding travels with the asset: the
        // range fetch delivers metadata + (zlib?) payload, the writer inflates
        asset.ct = entry.Flags;
        asset.cl = entry.CompressedSize;
        asset.ml = entry.MetadataSize;
        asset.mc = entry.Checksum;
        asset.blob = loose.blob;
        asset.blob_offset = entry.Offset;
        asset.tier = loose.tier;
        refined.assets.push_back(asset);
    }
    if (refined.assets.empty())
        return false;

    // deterministic synthesized layout (same as manifest-built packs): one
    // section, blocks packed in entry order after the table
    vfs_plan_layout_file(refined);

    const uint64_t original = file.size;
    const astr_t source_tail = loose.blob; // copied: `loose` moves with `file`
    file = std::move(refined);
    LOG_INFO(
        "refine: %s -> %zu block assets (range source kept: %s), %" PRIu64 " -> %" PRIu64
        " bytes, %zu transfers",
        file.name.c_str(),
        file.assets.size(),
        source_tail.c_str(),
        original,
        file.size,
        reader.transfers);
    return true;
}
} // namespace

bool vfs_manifest_refine_ranges(
    VfsPlan &plan, const fs_path &blobs_dir, const astr_t &file_repo, uint64_t min_size)
{
    size_t refined = 0;
    size_t candidates = 0;
    for (auto &file : plan.files)
    {
        if (file.is_uop || file.assets.size() != 1 || file.size < min_size)
            continue;
        astr_t lower = str_lower(file.name);
        if (lower.rfind(".uop") != lower.size() - 4)
            continue;
        ++candidates;
        const auto &asset = file.assets.front();
        if (asset.ct != 0)
        {
            LOG_WARN("refine: skipping %s (compressed source, ct=%u)", file.name.c_str(), asset.ct);
            continue; // compressed sources cannot be ranged (zlib is not seekable)
        }
        if (vfs_refine_file(file, blobs_dir, file_repo))
            ++refined;
    }
    LOG_INFO("refine: %zu of %zu candidates split into block range assets", refined, candidates);
    return true;
}

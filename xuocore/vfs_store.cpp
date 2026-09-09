// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

#define LOGGER_MODULE Vfs

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <map>

#if !defined(XUO_WINDOWS)
#include <unistd.h> // ftruncate
#endif

#include <vector>

#include "vfs_store.h"
#include "uop.h"

#include "common.h"
#include <common/logging/logging.h>
#include <common/utils.h>

#include <external/zlib_amalg.h>

// ---------------------------------------------------------------------------
// portable large-file io helpers
// ---------------------------------------------------------------------------

static bool vfs_file_seek(FILE *fp, uint64_t offset)
{
#if defined(XUO_WINDOWS)
    return _fseeki64(fp, __int64(offset), SEEK_SET) == 0;
#else
    return fseeko(fp, off_t(offset), SEEK_SET) == 0;
#endif
}

static bool vfs_file_truncate(FILE *fp, uint64_t size)
{
    fflush(fp);
#if defined(XUO_WINDOWS)
    return _chsize_s(_fileno(fp), __int64(size)) == 0;
#else
    return ftruncate(fileno(fp), off_t(size)) == 0;
#endif
}

static bool vfs_file_write_at(FILE *fp, uint64_t offset, const void *data, size_t size)
{
    if (!vfs_file_seek(fp, offset) || fwrite(data, 1, size, fp) != size)
        return false;
    return true;
}

static bool vfs_file_size(FILE *fp, uint64_t *out_size)
{
    if (fflush(fp) != 0)
        return false;
#if defined(XUO_WINDOWS)
    const __int64 cur = _ftelli64(fp);
    if (_fseeki64(fp, 0, SEEK_END) != 0)
        return false;
    const __int64 size = _ftelli64(fp);
    const bool ok = _fseeki64(fp, cur, SEEK_SET) == 0 && size >= 0;
#else
    const off_t cur = ftello(fp);
    if (fseeko(fp, 0, SEEK_END) != 0)
        return false;
    const off_t size = ftello(fp);
    const bool ok = fseeko(fp, cur, SEEK_SET) == 0 && size >= 0;
#endif
    *out_size = uint64_t(size);
    return ok;
}

// reads exactly `expected` bytes from the blob store; 0 = ok, 1 = missing,
// 2 = size mismatch (the blob store is content-addressed, sizes must match)
static int
vfs_store_read_blob(const fs_path &blobs_dir, const VfsAsset &asset, std::vector<uint8_t> &out)
{
    const uint64_t expected = asset.ct == 0 ? uint64_t(asset.ul) : uint64_t(asset.cl);
    const auto path = fs_path_join(blobs_dir, asset.blob);
    FILE *fp = fopen(fs_path_ascii(path), "rb");
    if (!fp)
    {
        LOG_TRACE("blob missing: %s", asset.blob.c_str());
        return 1;
    }
    uint64_t size = 0;
    if (!vfs_file_size(fp, &size))
    {
        fclose(fp);
        return 2;
    }
    if (size != expected)
    {
        LOG_ERROR(
            "blob size mismatch: %s (got %" PRIu64 ", expected %" PRIu64 ")",
            asset.blob.c_str(),
            size,
            expected);
        fclose(fp);
        return 2;
    }
    out.resize(size_t(expected));
    const size_t read = size ? fread(out.data(), 1, out.size(), fp) : 0;
    fclose(fp);
    if (read != out.size())
        return 2;
    return 0;
}

// ---------------------------------------------------------------------------
// pack assembly (§10.1, §10.2)
// ---------------------------------------------------------------------------

static uint64_t vfs_store_table_pos(size_t asset_index)
{
    return sizeof(UopHeader) + sizeof(UopSection) + sizeof(UopFileEntry) * asset_index;
}

// synthesized metadata (§10.2): ml bytes, the first 4 being the type-3 chain
// header { type = 3, size = ml - 4 } (LE), the remainder zeros; ml == 0 means
// the block starts directly with the payload
static void vfs_store_synthesize_metadata(const VfsAsset &asset, std::vector<uint8_t> &out)
{
    out.assign(asset.ml, 0);
    if (asset.ml >= sizeof(UopFileMetadata))
    {
        UopFileMetadata meta;
        meta.Type = 3;
        meta.Size = uint16_t(asset.ml - sizeof(UopFileMetadata));
        memcpy(out.data(), &meta, sizeof(meta));
    }
}

// 32-bit fingerprint of a pack's planned table: every entry hashed in table
// order (uop hash + planned offset + ml + cl + ul). Written into
// UopHeader::Signature when the pack is created; on open, a mismatch means
// the on-disk table was built for a different plan and must be recreated.
static uint32_t vfs_store_plan_fingerprint(const VfsPlan::VfsFile &file)
{
    std::vector<uint8_t> bytes;
    bytes.reserve(file.assets.size() * (sizeof(uint64_t) * 2 + sizeof(uint32_t) * 3));
    const auto push = [&bytes](const void *data, size_t size)
    {
        const auto *p = static_cast<const uint8_t *>(data);
        bytes.insert(bytes.end(), p, p + size);
    };
    for (const auto &asset : file.assets)
    {
        const uint64_t hash = asset.uop_hash;
        const uint64_t offset = asset.file_offset;
        const uint32_t ml = asset.ml;
        const uint32_t compressed = asset.ct == 0 ? uint32_t(asset.ul) : uint32_t(asset.cl);
        const uint32_t ul = uint32_t(asset.ul);
        push(&hash, sizeof(hash));
        push(&offset, sizeof(offset));
        push(&ml, sizeof(ml));
        push(&compressed, sizeof(compressed));
        push(&ul, sizeof(ul));
    }
    return crc32_checksum(bytes.data(), bytes.size());
}

// true when the on-disk pack is missing or was not built for this plan:
// same total size is not enough, the Signature fingerprint tells plans apart
static bool vfs_store_pack_recreate_needed(
    const fs_path &path, const VfsPlan::VfsFile &file, uint32_t fingerprint)
{
    if (!fs_path_exists(path))
        return true;
    FILE *probe = fopen(fs_path_ascii(path), "rb");
    if (!probe)
        return true;
    UopHeader header;
    uint64_t existing = 0;
    const bool compatible = vfs_file_size(probe, &existing) && existing == file.size &&
                            fread(&header, sizeof(header), 1, probe) == 1 &&
                            header.Magic == MYP_MAGIC && header.Signature == fingerprint;
    fclose(probe);
    return !compatible;
}

// opens the pack for block writes; creates the full header + section + entry
// table from the plan when the file is missing or built for a different plan
static FILE *vfs_store_pack_open(const fs_path &path, const VfsPlan::VfsFile &file, bool *created)
{
    *created = false;
    fs_path_create(fs_directory(path));

    const uint32_t fingerprint = vfs_store_plan_fingerprint(file);
    const bool recreate = vfs_store_pack_recreate_needed(path, file, fingerprint);

    FILE *fp = fopen(fs_path_ascii(path), recreate ? "wb+" : "r+b");
    if (!fp)
    {
        LOG_ERROR("could not open pack: %s", fs_path_ascii(path));
        return nullptr;
    }
    if (!recreate)
        return fp;
    *created = true;
    LOG_TRACE("creating pack: %s (%" PRIu64 " bytes)", fs_path_ascii(path), file.size);

    UopHeader header;
    header.Magic = MYP_MAGIC;
    header.Version = 4;
    header.Signature = fingerprint;
    header.SectionOffset = sizeof(UopHeader);
    header.FileCapacity = uint32_t(file.assets.size());
    header.FileCount = uint32_t(file.assets.size());
    header.SectionCount = 1;
    header.Unk1 = 1;
    header.Unk2 = 0;

    UopSection section;
    section.FileCount = uint32_t(file.assets.size());
    section.NextSection = 0;

    std::vector<uint8_t> table;
    table.reserve(sizeof(header) + sizeof(section) + sizeof(UopFileEntry) * file.assets.size());
    const auto push = [&table](const void *data, size_t size)
    {
        const auto *bytes = static_cast<const uint8_t *>(data);
        table.insert(table.end(), bytes, bytes + size);
    };
    push(&header, sizeof(header));
    push(&section, sizeof(section));
    for (const auto &asset : file.assets)
    {
        // Offset stays 0 until the block is written: UopLoadFile skips the
        // entry, and the patch below flips it to readable in place
        UopFileEntry entry;
        entry.Offset = 0;
        entry.MetadataSize = asset.ml;
        entry.CompressedSize = uint32_t(asset.ct == 0 ? asset.ul : asset.cl);
        entry.DecompressedSize = uint32_t(asset.ul);
        entry.Hash = asset.uop_hash;
        entry.Checksum = asset.mc;
        entry.Flags = asset.ct == 0 ? uint16_t(0) : uint16_t(1);
        push(&entry, sizeof(entry));
    }
    if (!vfs_file_write_at(fp, 0, table.data(), table.size()) || !vfs_file_truncate(fp, file.size))
    {
        LOG_ERROR("could not initialize pack: %s", fs_path_ascii(path));
        fclose(fp);
        return nullptr;
    }
    return fp;
}

// writes [metadata][blob] at the planned offset and patches the entry's
// Offset in place (data first, pointer second: a crash in between leaves the
// entry skipped and the block simply rewritten later)
static bool vfs_store_pack_write_block(
    FILE *fp,
    const VfsPlan::VfsFile &file,
    size_t asset_index,
    const VfsAsset &asset,
    const std::vector<uint8_t> &blob)
{
    std::vector<uint8_t> metadata;
    vfs_store_synthesize_metadata(asset, metadata);

    if (!vfs_file_write_at(fp, asset.file_offset, metadata.data(), metadata.size()))
        return false;
    if (!blob.empty() &&
        !vfs_file_write_at(fp, asset.file_offset + asset.ml, blob.data(), blob.size()))
        return false;

    UopFileEntry patch;
    patch.Offset = asset.file_offset;
    if (!vfs_file_write_at(
            fp, vfs_store_table_pos(asset_index), &patch.Offset, sizeof(patch.Offset)))
        return false;
    return fflush(fp) == 0;
}

// ---------------------------------------------------------------------------
// loose files: materialized whole from the blob (decompressed when ct == 1)
// ---------------------------------------------------------------------------

static bool vfs_store_write_loose(
    const vfs_store_params &params, const VfsAsset &asset, const std::vector<uint8_t> &blob)
{
    const auto path = fs_path_join(params.install_dir, asset.name);
    fs_path_create(fs_directory(path));

    std::vector<uint8_t> payload;
    const uint8_t *data = blob.data();
    size_t size = blob.size();
    if (asset.ct == 1)
    {
        payload.resize(asset.ul);
        uLongf dst_len = payload.size();
        auto *src = reinterpret_cast<Bytef const *>(blob.data());
        if (uncompress(payload.data(), &dst_len, src, uLong(blob.size())) != Z_OK)
        {
            LOG_ERROR("loose blob failed to decompress: %s", asset.blob.c_str());
            return false;
        }
        payload.resize(size_t(dst_len));
        data = payload.data();
        size = payload.size();
    }
    return fs_file_write(fs_path_ascii(path), data, size);
}

// ---------------------------------------------------------------------------
// materialization core, shared by the batch driver and the sparse backend
// ---------------------------------------------------------------------------

struct vfs_store_context
{
    const VfsPlan *plan = nullptr;
    vfs_store_params params;
    std::vector<uint8_t> *states = nullptr; // one byte per plan asset
    vfs_journal *journal = nullptr;         // optional completion marks
    vfs_materialize_stats *stats = nullptr; // optional counters
};

static void
vfs_store_count(vfs_store_context &ctx, size_t vfs_materialize_stats::*field, int delta = 1)
{
    if (ctx.stats)
        (ctx.stats->*field) += size_t(delta);
}

static void vfs_store_mark(vfs_store_context &ctx, char op, uint32_t fi, uint32_t ai)
{
    if (!ctx.journal)
        return;
    vfs_journal_record record;
    record.op = op;
    record.file_index = fi;
    record.asset_index = ai;
    if (!vfs_journal_mark(*ctx.journal, record))
        LOG_WARN("journal mark failed: %c %u %u", op, fi, ai);
}

static size_t vfs_store_global_index(const VfsPlan &plan, size_t file_index, size_t asset_index)
{
    size_t index = asset_index;
    for (size_t i = 0; i < file_index; ++i)
        index += plan.files[i].assets.size();
    return index;
}

// materializes one asset from its blob; returns true when the asset bytes are
// in place (already present counts), false when the blob is missing or bad
static bool
vfs_store_materialize_asset(vfs_store_context &ctx, size_t file_index, size_t asset_index)
{
    const auto &file = ctx.plan->files[file_index];
    const auto &asset = file.assets[asset_index];
    const size_t gi = vfs_store_global_index(*ctx.plan, file_index, asset_index);

    if (vfs_asset_is_hole(asset))
    {
        // reserved entry: no blob exists, the table slot is all we owe
        if ((*ctx.states)[gi] == vfs_pending)
            (*ctx.states)[gi] = vfs_fetched;
        vfs_store_count(ctx, &vfs_materialize_stats::assets_hole);
        return true;
    }
    if ((*ctx.states)[gi] == vfs_fetched)
    {
        vfs_store_count(ctx, &vfs_materialize_stats::assets_present);
        return true;
    }

    std::vector<uint8_t> blob;
    const int res = vfs_store_read_blob(ctx.params.blobs_dir, asset, blob);
    if (res == 1)
    {
        vfs_store_count(ctx, &vfs_materialize_stats::assets_missing);
        return false; // stays pending: M2's fetch list
    }
    if (res != 0)
    {
        (*ctx.states)[gi] = vfs_failed;
        vfs_store_count(ctx, &vfs_materialize_stats::assets_failed);
        return false;
    }

    bool ok = false;
    if (file.is_uop)
    {
        bool created = false;
        FILE *fp =
            vfs_store_pack_open(fs_path_join(ctx.params.install_dir, file.name), file, &created);
        if (fp)
        {
            ok = vfs_store_pack_write_block(fp, file, asset_index, asset, blob);
            fclose(fp);
        }
        if (!ok)
            LOG_ERROR("failed to write block %zu of %s", asset_index, file.name.c_str());
    }
    else
    {
        ok = vfs_store_write_loose(ctx.params, asset, blob);
        if (!ok)
            LOG_ERROR("failed to write loose file: %s", asset.name.c_str());
    }

    if (!ok)
    {
        (*ctx.states)[gi] = vfs_failed;
        vfs_store_count(ctx, &vfs_materialize_stats::assets_failed);
        return false;
    }

    (*ctx.states)[gi] = vfs_fetched;
    vfs_store_mark(ctx, 'F', uint32_t(file_index), uint32_t(asset_index));
    vfs_store_count(ctx, &vfs_materialize_stats::assets_fetched);
    if (ctx.stats)
        ctx.stats->bytes_written += vfs_asset_block_size(asset);
    return true;
}

// seals files that reached 100% of their expected (non-hole) assets
static void vfs_store_seal(vfs_store_context &ctx)
{
    size_t asset_index = 0;
    for (size_t fi = 0; fi < ctx.plan->files.size(); ++fi)
    {
        const auto &file = ctx.plan->files[fi];
        bool complete = !file.assets.empty();
        for (size_t ai = 0; ai < file.assets.size(); ++ai, ++asset_index)
        {
            if (!vfs_asset_is_hole(file.assets[ai]) && (*ctx.states)[asset_index] != vfs_fetched)
                complete = false;
        }
        if (complete)
        {
            vfs_store_mark(ctx, 'S', uint32_t(fi), 0);
            // durability: the file just reached its final bytes this run, so
            // flush them to stable storage before reporting it sealed
            if (!vfs_state_file_sync(fs_path_join(ctx.params.install_dir, file.name)))
                LOG_WARN("could not flush sealed file: %s", file.name.c_str());
        }
    }
}

bool vfs_store_materialize(
    const VfsPlan &plan,
    const vfs_store_params &params,
    std::vector<uint8_t> &states,
    vfs_journal *journal,
    vfs_materialize_stats *stats)
{
    vfs_materialize_stats run;
    vfs_store_context ctx;
    ctx.plan = &plan;
    ctx.params = params;
    ctx.states = &states;
    ctx.journal = journal;
    ctx.stats = &run;

    // the caller may hand us a fresh vector; the plan fixes the size
    states.resize(vfs_state_asset_count(plan), vfs_pending);

    // a pack that is about to be recreated gets an empty table: whatever the
    // state DB believed about its blocks is void, so reset the file's states
    // and let every asset go through the write path in this run (otherwise
    // the run would "seal" a pack whose table has no readable entries)
    for (size_t fi = 0; fi < plan.files.size(); ++fi)
    {
        const auto &file = plan.files[fi];
        if (!file.is_uop || file.assets.empty())
            continue;
        const auto path = fs_path_join(params.install_dir, file.name);
        if (!vfs_store_pack_recreate_needed(path, file, vfs_store_plan_fingerprint(file)))
            continue;
        const size_t base = vfs_store_global_index(plan, fi, 0);
        for (size_t ai = 0; ai < file.assets.size(); ++ai)
            states[base + ai] = vfs_pending;
        LOG_INFO("recreating pack for plan: %s", file.name.c_str());
    }

    fs_path_create(params.install_dir);

    for (size_t fi = 0; fi < plan.files.size(); ++fi)
    {
        for (size_t ai = 0; ai < plan.files[fi].assets.size(); ++ai)
        {
            ++run.assets_total;
            vfs_store_materialize_asset(ctx, fi, ai);
        }
    }
    vfs_store_seal(ctx);

    if (stats)
        *stats = run;
    LOG_INFO(
        "materialize: %zu assets (%zu written, %zu present, %zu missing, %zu failed, %" PRIu64
        " bytes)",
        run.assets_total,
        run.assets_fetched,
        run.assets_present,
        run.assets_missing,
        run.assets_failed,
        run.bytes_written);
    return run.assets_failed == 0;
}

// ---------------------------------------------------------------------------
// verification
// ---------------------------------------------------------------------------

static bool vfs_verify_zlib(
    const uint8_t *stored, size_t stored_size, const VfsAsset &asset, std::vector<uint8_t> &scratch)
{
    if (asset.ct != 1)
        return true;
    scratch.resize(asset.ul);
    uLongf dst_len = scratch.size();
    auto *src = reinterpret_cast<Bytef const *>(stored);
    if (uncompress(scratch.data(), &dst_len, src, uLong(stored_size)) != Z_OK)
        return false;
    return dst_len == asset.ul;
}

static bool vfs_store_blob_exists(const vfs_store_params &params, const VfsAsset &asset)
{
    const auto path = fs_path_join(params.blobs_dir, asset.blob);
    return fs_path_exists(path);
}

static uint8_t vfs_verify_pack_asset(
    const vfs_store_params &params,
    const uint8_t *start,
    uint64_t file_size,
    size_t asset_index,
    const VfsAsset &asset,
    std::vector<uint8_t> &scratch)
{
    if (vfs_asset_is_hole(asset))
        return vfs_verify_ok;

    if (!start || file_size < vfs_store_table_pos(asset_index) + sizeof(UopFileEntry))
        return vfs_store_blob_exists(params, asset) ? vfs_verify_missing_block :
                                                      vfs_verify_missing_blob;

    UopFileEntry entry;
    memcpy(&entry, start + vfs_store_table_pos(asset_index), sizeof(entry));
    if (entry.Offset == 0) // hole in the materialized file
        return vfs_store_blob_exists(params, asset) ? vfs_verify_missing_block :
                                                      vfs_verify_missing_blob;

    const uint32_t compressed = asset.ct == 0 ? uint32_t(asset.ul) : uint32_t(asset.cl);
    const uint16_t flags = asset.ct == 0 ? uint16_t(0) : uint16_t(1);
    if (entry.Offset != asset.file_offset || entry.MetadataSize != asset.ml ||
        entry.CompressedSize != compressed || entry.DecompressedSize != asset.ul ||
        entry.Hash != asset.uop_hash || entry.Checksum != asset.mc || entry.Flags != flags)
        return vfs_verify_corrupt;
    if (entry.Offset + entry.MetadataSize + entry.CompressedSize > file_size)
        return vfs_verify_corrupt;
    if (!entry.CompressedSize)
        return vfs_verify_corrupt;

    // stored (ct == 0) blobs carry the payload verbatim; zlib blobs must
    // decompress to exactly ul bytes
    const uint8_t *stored = start + entry.Offset + entry.MetadataSize;
    if (!vfs_verify_zlib(stored, entry.CompressedSize, asset, scratch))
        return vfs_verify_corrupt;
    return vfs_verify_ok;
}

static uint8_t vfs_verify_loose_asset(
    const vfs_store_params &params, const VfsAsset &asset, std::vector<uint8_t> &scratch)
{
    (void)scratch;
    if (vfs_asset_is_hole(asset))
        return vfs_verify_ok;

    const auto path = fs_path_join(params.install_dir, asset.name);
    FILE *fp = fopen(fs_path_ascii(path), "rb");
    if (!fp)
        return vfs_store_blob_exists(params, asset) ? vfs_verify_missing_block :
                                                      vfs_verify_missing_blob;

    // the loose file is the whole decompressed payload: size is the spec
    // check, the blob (when present) allows an exact content cross-check
    uint64_t size = 0;
    if (!vfs_file_size(fp, &size) || size != asset.ul)
    {
        fclose(fp);
        return vfs_verify_corrupt;
    }
    std::vector<uint8_t> stored;
    stored.resize(size_t(size));
    const size_t read = size ? fread(stored.data(), 1, stored.size(), fp) : 0;
    fclose(fp);
    if (read != stored.size())
        return vfs_verify_corrupt;

    std::vector<uint8_t> blob;
    if (vfs_store_read_blob(params.blobs_dir, asset, blob) == 0)
    {
        std::vector<uint8_t> payload;
        if (asset.ct == 1)
        {
            payload.resize(asset.ul);
            uLongf dst_len = payload.size();
            auto *src = reinterpret_cast<Bytef const *>(blob.data());
            if (uncompress(payload.data(), &dst_len, src, uLong(blob.size())) != Z_OK)
            {
                // the plan cannot be confirmed from content: the size check
                // above is the only verdict for this asset in this pass
                LOG_WARN("verify: unusable blob for %s, size check only", asset.name.c_str());
                return vfs_verify_ok; // blob unusable: size check only
            }
            payload.resize(size_t(dst_len));
        }
        else
        {
            payload = blob;
        }
        if (payload == stored)
            return vfs_verify_ok;
        return vfs_verify_corrupt;
    }
    return vfs_verify_ok;
}

// on-disk truth check shared with the state DB reconcile: the block bytes of
// `asset` are present in `file` exactly as planned
bool vfs_store_pack_asset_present(
    const uint8_t *start, uint64_t file_size, size_t asset_index, const VfsAsset &asset)
{
    if (!start || file_size < vfs_store_table_pos(asset_index) + sizeof(UopFileEntry))
        return false;
    if (vfs_asset_is_hole(asset))
        return true; // holes are satisfied by the reserved table slot
    UopFileEntry entry;
    memcpy(&entry, start + vfs_store_table_pos(asset_index), sizeof(entry));
    const uint32_t compressed = asset.ct == 0 ? uint32_t(asset.ul) : uint32_t(asset.cl);
    const uint16_t flags = asset.ct == 0 ? uint16_t(0) : uint16_t(1);
    return entry.Offset == asset.file_offset && entry.Offset != 0 &&
           entry.MetadataSize == asset.ml && entry.CompressedSize == compressed &&
           entry.DecompressedSize == asset.ul && entry.Hash == asset.uop_hash &&
           entry.Flags == flags &&
           entry.Offset + entry.MetadataSize + entry.CompressedSize <= file_size;
}

int vfs_store_verify(const VfsPlan &plan, const vfs_store_params &params, vfs_verify_result *out)
{
    vfs_verify_result result;
    int corrupt = 0;
    for (const auto &file : plan.files)
    {
        vfs_verify_file vf;
        vf.name = file.name;
        vf.is_uop = file.is_uop;

        const uint8_t *mapped = nullptr;
        uint64_t mapped_size = 0;
        size_t map_size = 0;
        if (file.is_uop)
        {
            const auto path = fs_path_join(params.install_dir, file.name);
            if (fs_path_exists(path))
                mapped = fs_map(path, &map_size);
            mapped_size = uint64_t(map_size);
        }

        for (size_t ai = 0; ai < file.assets.size(); ++ai)
        {
            const auto &asset = file.assets[ai];
            std::vector<uint8_t> scratch;
            vfs_verify_asset va;
            va.uop_hash = asset.uop_hash;
            va.name = asset.name;
            va.status = file.is_uop ?
                            vfs_verify_pack_asset(params, mapped, mapped_size, ai, asset, scratch) :
                            vfs_verify_loose_asset(params, asset, scratch);
            ++vf.counts[va.status];
            if (va.status == vfs_verify_corrupt)
            {
                ++corrupt;
                result.corrupted = true;
            }
            vf.assets.emplace_back(va);
        }
        if (mapped)
            fs_unmap(const_cast<unsigned char *>(mapped), map_size);

        vf.sealed = vf.counts[vfs_verify_missing_blob] == 0 &&
                    vf.counts[vfs_verify_missing_block] == 0 && vf.counts[vfs_verify_corrupt] == 0;
        result.files.emplace_back(std::move(vf));
    }
    if (out)
        *out = std::move(result);
    return corrupt;
}

// ---------------------------------------------------------------------------
// the M1 native backend: store_sparse
// ---------------------------------------------------------------------------

namespace
{
class store_sparse : public vfs_store_backend
{
public:
    explicit store_sparse(const vfs_store_params &params)
        : m_params(params)
    {
    }

    ~store_sparse() override
    {
        if (m_journal.handle)
            vfs_journal_close(m_journal);
    }

    bool open(const VfsPlan &plan) override
    {
        m_plan = &plan;
        m_states.assign(vfs_state_asset_count(plan), vfs_pending);
        std::vector<vfs_journal_record> records;
        vfs_journal_replay(journal_path(), records);
        vfs_state_apply_journal(plan, records, m_states);
        for (size_t fi = 0; fi < plan.files.size(); ++fi)
        {
            const auto &file = plan.files[fi];
            for (size_t ai = 0; ai < file.assets.size(); ++ai)
            {
                const auto &asset = file.assets[ai];
                if (vfs_asset_is_loose(asset))
                    m_loose_index[asset.name] = { fi, ai };
                else
                    m_pack_index[asset.uop_hash] = { fi, ai };
            }
        }
        fs_path_create(m_params.install_dir);
        const auto journal_dir = fs_directory(journal_path());
        fs_path_create(journal_dir);
        if (!vfs_journal_open(m_journal, journal_path()))
        {
            LOG_ERROR("could not open journal: %s", fs_path_ascii(journal_path()));
            return false;
        }
        return true;
    }

    bool ensure(const VfsAsset &asset, bool block) override
    {
        (void)block; // M1 always materializes synchronously
        size_t fi = 0, ai = 0;
        if (!locate(asset, fi, ai))
            return false;
        vfs_store_context ctx = context();
        return vfs_store_materialize_asset(ctx, fi, ai);
    }

    void prefetch(const VfsAsset &asset, int prio) override
    {
        (void)asset;
        (void)prio; // M2: fetch engine queues the blob download
    }

    void evict(size_t bytes_target) override
    {
        (void)bytes_target; // web arena only (§17.3); disk files are never evicted
    }

    bool is_ready(const VfsAsset &asset) const override
    {
        size_t fi = 0, ai = 0;
        if (!locate(asset, fi, ai))
            return false;
        return m_states[vfs_store_global_index(*m_plan, fi, ai)] == vfs_fetched;
    }

    void seal_check() override
    {
        vfs_store_context ctx = context();
        vfs_store_seal(ctx);
    }

private:
    fs_path journal_path() const
    {
        if (fs_path_some(m_params.state_dir))
            return fs_path_join(m_params.state_dir, "journal.log");
        return fs_path_join(m_params.install_dir, "cache", "state", "journal.log");
    }

    vfs_store_context context()
    {
        vfs_store_context ctx;
        ctx.plan = m_plan;
        ctx.params = m_params;
        ctx.states = &m_states;
        ctx.journal = &m_journal;
        ctx.stats = nullptr;
        return ctx;
    }

    bool locate(const VfsAsset &asset, size_t &fi, size_t &ai) const
    {
        if (!m_plan)
            return false;
        if (vfs_asset_is_loose(asset))
        {
            const auto found = m_loose_index.find(asset.name);
            if (found == m_loose_index.end())
                return false;
            fi = found->second.first;
            ai = found->second.second;
            return true;
        }
        const auto found = m_pack_index.find(asset.uop_hash);
        if (found == m_pack_index.end())
            return false;
        fi = found->second.first;
        ai = found->second.second;
        return true;
    }

    vfs_store_params m_params;
    const VfsPlan *m_plan = nullptr;
    std::vector<uint8_t> m_states;
    vfs_journal m_journal;
    std::map<uint64_t, std::pair<size_t, size_t>> m_pack_index; // uop_hash -> file/asset
    std::map<astr_t, std::pair<size_t, size_t>> m_loose_index;  // name -> file/asset
};
} // namespace

vfs_store_backend *vfs_store_backend_sparse_create(const vfs_store_params &params)
{
    // the backend binds to a plan through open() before first use
    return new store_sparse(params);
}

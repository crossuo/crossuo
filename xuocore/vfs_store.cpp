// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

#define LOGGER_MODULE Vfs

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <atomic>
#include <chrono>
#include <map>

#if !defined(XUO_WINDOWS)
#include <unistd.h> // ftruncate
#include <sys/stat.h>
#else
#include <windows.h>
#include <io.h> // _chsize_s/_commit
#endif

#include <vector>

#include "vfs_store.h"
#include "vfs_store_arena.h"
#include "vfs_record.h"
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
    return _fseeki64(fp, (long long)(offset), SEEK_SET) == 0;
#else
    return fseeko(fp, off_t(offset), SEEK_SET) == 0;
#endif
}

static bool vfs_file_truncate(FILE *fp, uint64_t size)
{
    fflush(fp);
#if defined(XUO_WINDOWS)
    return _chsize_s(_fileno(fp), (long long)(size)) == 0;
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

// reads the asset's source bytes from the blob store; 0 = ok, 1 = missing,
// 2 = size mismatch / unusable. Range assets read their block window
// ([ml metadata][payload] at blob_offset) instead of the whole blob
static int
vfs_store_read_blob(const fs_path &blobs_dir, const VfsAsset &asset, std::vector<uint8_t> &out)
{
    const bool ranged = asset.blob_offset != 0;
    uint64_t expected =
        ranged ? vfs_asset_range_len(asset) : (asset.ct == 0 ? uint64_t(asset.ul) : uint64_t(asset.cl));
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
    if (ranged)
    {
        // the source file is bigger than the block; only the window matters
        if (size < asset.blob_offset + expected)
        {
            LOG_ERROR(
                "blob too small for range: %s (got %" PRIu64 ", need %" PRIu64 "+%" PRIu64 ")",
                asset.blob.c_str(),
                size,
                asset.blob_offset,
                expected);
            fclose(fp);
            return 2;
        }
        if (fseeko(fp, off_t(asset.blob_offset), SEEK_SET) != 0)
        {
            fclose(fp);
            return 2;
        }
    }
    else if (size != expected)
    {
#if defined(XUO_EMSC)
        // web: whole-file transfers may be served by a normalizing proxy in
        // rebuilt (all blocks stored) form, which is larger than the
        // manifest's original decompressed size. Accept the bigger artifact
        // whole (the payload is the entire file as stored).
        if (!ranged && size > expected)
        {
            expected = size;
        }
        else
#endif
        {
            LOG_ERROR(
                "blob size mismatch: %s (got %" PRIu64 ", expected %" PRIu64 ")",
                asset.blob.c_str(),
                size,
                expected);
            fclose(fp);
            return 2;
        }
    }
    out.resize(size_t(expected));
    const size_t read = size ? fread(out.data(), 1, out.size(), fp) : 0;
    fclose(fp);
    if (read != out.size())
        return 2;
    return 0;
}

// ---------------------------------------------------------------------------
// pack assembly (UOP table layout that matches UopLoadFile expectations)
// ---------------------------------------------------------------------------

static uint64_t vfs_store_table_pos(size_t asset_index)
{
    return sizeof(UopHeader) + sizeof(UopSection) + sizeof(UopFileEntry) * asset_index;
}

// synthesized metadata: ml bytes, the first 4 being the type-3 chain
// header { type = 3, size = ml - 4 } (LE), the remainder zeros (no consumer
// validates the content - the game only walks the chain); ml == 0 means
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
// order (uop hash + planned offset + ml + stored ul). Written into
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
        const uint32_t stored = uint32_t(asset.ul);
        push(&hash, sizeof(hash));
        push(&offset, sizeof(offset));
        push(&ml, sizeof(ml));
        push(&stored, sizeof(stored));
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

// ---------------------------------------------------------------------------
// sparse-filesystem probe: the planned-offset layout
// depends on the filesystem growing files sparsely. On fat/exFAT every
// planned byte is really preallocated; detected once after the first pack is
// created (st_blocks probe / windows attribute) and warned about loudly.
// The plan's append-layout fallback (Offset=0 table entries, blocks appended
// as downloaded and patched in place) is deliberately NOT implemented - the
// trap depends on static planned offsets; see docs/NOTES.md.
// ---------------------------------------------------------------------------

static bool vfs_store_sparse_checked = false;
static bool vfs_store_sparse_ok = true;

bool vfs_store_fs_supports_sparse(const fs_path &probe_dir)
{
    if (fs_path_empty(probe_dir))
        return true;
    fs_path_create(probe_dir);
    const auto probe = fs_path_join(probe_dir, ".vfs_sparse_probe");
    FILE *fp = fopen(fs_path_ascii(probe), "wb");
    if (!fp)
        return true; // cannot probe: assume the best, the write path fails loudly anyway
    const bool sized = vfs_file_truncate(fp, 1 << 20); // 1 MB hole
    fclose(fp);
    bool sparse = true;
    if (sized)
    {
#if defined(XUO_WINDOWS)
        const DWORD attrs = GetFileAttributesA(fs_path_ascii(probe));
        sparse = attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_SPARSE_FILE) != 0;
#else
        struct stat st;
        sparse = stat(fs_path_ascii(probe), &st) != 0 ||
                 uint64_t(st.st_blocks) * 512 < uint64_t(st.st_size);
#endif
    }
    fs_del(probe);
    return sparse;
}

// probes once per process and warns loudly when the install would really
// preallocate its planned bytes
static void vfs_store_check_sparse(uint64_t planned_bytes, const fs_path &dir)
{
    if (vfs_store_sparse_checked)
        return;
    vfs_store_sparse_checked = true;
    vfs_store_sparse_ok = vfs_store_fs_supports_sparse(dir);
    if (!vfs_store_sparse_ok)
        LOG_WARN(
            "filesystem does not support sparse files; up to %" PRIu64
            " MB will be preallocated",
            planned_bytes / (1024 * 1024));
}

// web: writes ONLY the header + section + entry table (a few hundred KB)
// and stops there - the game builds its block hashmap from the table, and
// every block read is intercepted by the punctual fetch layer, so the full
// planned size never exists on disk or in memory. MEMFS files live in the
// browser JS heap: a full-size pack would allocate it whole (the OOM).
// Returns the skeleton size through *out_size; the mapping and every block
// offset stay the plan's virtual layout.
bool vfs_store_materialize_skeleton(
    const VfsPlan &plan, size_t file_index, const fs_path &install_dir, uint64_t *out_size)
{
    *out_size = 0;
    if (file_index >= plan.files.size())
        return false;
    const auto &file = plan.files[file_index];
    if (!file.is_uop || file.assets.empty())
        return false;
    const fs_path path = fs_path_join(install_dir, file.name);
    fs_path_create(fs_directory(path));
    const uint32_t fingerprint = vfs_store_plan_fingerprint(file);

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
        // the planned layout travels in the table so the punctual fetch can
        // relocate any block by its entry alone; Offset points into the
        // VIRTUAL pack (past the skeleton end) and is never dereferenced -
        // reads go through vfs_asset_fetch_block
        UopFileEntry entry;
        entry.Offset = vfs_asset_is_hole(asset) ? 0 : asset.file_offset;
        entry.MetadataSize = asset.ml;
        entry.CompressedSize = uint32_t(asset.ul);
        entry.DecompressedSize = uint32_t(asset.ul);
        entry.Hash = asset.uop_hash;
        entry.Checksum = asset.mc;
        entry.Flags = uint16_t(asset.ct);
        push(&entry, sizeof(entry));
    }

    FILE *fp = fopen(fs_path_ascii(path), "wb");
    if (fp == nullptr)
    {
        LOG_ERROR("web skeleton: cannot create %s", fs_path_ascii(path));
        return false;
    }
    const bool wrote = fwrite(table.data(), 1, table.size(), fp) == table.size();
    fclose(fp);
    if (!wrote)
    {
        LOG_ERROR("web skeleton: write failed: %s", fs_path_ascii(path));
        return false;
    }
    *out_size = table.size();
    return true;
}

// opens the pack for block writes; creates the full header + section + entry
// table from the plan when the file is missing or built for a different plan
static FILE *vfs_store_pack_open(const fs_path &path, const VfsPlan::VfsFile &file, bool *created)
{
    *created = false;
    fs_path_create(fs_directory(path));

    const uint32_t fingerprint = vfs_store_plan_fingerprint(file);
    const bool recreate = vfs_store_pack_recreate_needed(path, file, fingerprint);
    if (recreate)
    {
        // a recreate throws away every materialized block of the pack: say
        // why, so a fingerprint/plan mismatch is never a silent wipe. A
        // missing (or empty) file is the fresh-install create - nothing was
        // thrown away and the create itself is traced below
        uint64_t existing_size = 0;
        uint32_t existing_sig = 0;
        if (FILE *probe = fopen(fs_path_ascii(path), "rb"))
        {
            UopHeader h;
            if (fread(&h, sizeof(h), 1, probe) == 1)
                existing_sig = h.Signature;
            vfs_file_size(probe, &existing_size);
            fclose(probe);
        }
        if (existing_size != 0)
        {
            LOG_INFO(
                "recreating pack %s (size %llu, sig %08x) for plan (size %llu, sig %08x)",
                fs_path_ascii(path),
                (unsigned long long)existing_size,
                existing_sig,
                (unsigned long long)file.size,
                fingerprint);
        }
    }

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
        // entry, and the patch below flips it to readable in place. Blocks
        // materialize stored (Flags == 0, CompressedSize == ul): raw-pointer
        // readers expect decompressed bytes, like EA's own local archives
        UopFileEntry entry;
        entry.Offset = 0;
        entry.MetadataSize = asset.ml;
        entry.CompressedSize = uint32_t(asset.ul);
        entry.DecompressedSize = uint32_t(asset.ul);
        entry.Hash = asset.uop_hash;
        entry.Checksum = asset.mc;
        entry.Flags = 0;
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

// writes [metadata][stored payload] at the planned offset and patches the
// entry in place (data first, pointer second: a crash in between leaves the
// entry skipped or stale and the block simply rewritten later). The payload
// is the blob's decompressed content: packs materialize stored blocks so the
// game's raw-pointer readers (art, map blocks) see the bytes EA's own local
// archives carry. The WHOLE entry is rewritten, not just Offset: stubs land
// as Flags == 0 blocks of the same planned shape, and the real block must
// restore the plan's fields when it heals (the stub stays on disk - and
// reads as absent - until then).
static bool vfs_store_pack_write_block(
    FILE *fp,
    const VfsPlan::VfsFile &file,
    size_t asset_index,
    const VfsAsset &asset,
    const std::vector<uint8_t> &blob)
{
    (void)file;
    // range assets carry [ml metadata][payload] from the source file: the
    // metadata is transplanted byte-perfect (the entry Checksum travels with
    // it); whole-blob assets synthesize the metadata like always
    std::vector<uint8_t> metadata;
    const uint8_t *payload_src = blob.data();
    size_t payload_len = blob.size();
    if (asset.blob_offset != 0)
    {
        if (blob.size() < asset.ml)
        {
            LOG_ERROR("range block shorter than metadata: %s", asset.blob.c_str());
            return false;
        }
        metadata.assign(blob.data(), blob.data() + asset.ml);
        payload_src = blob.data() + asset.ml;
        payload_len = blob.size() - asset.ml;
    }
    else
    {
        vfs_store_synthesize_metadata(asset, metadata);
    }

    std::vector<uint8_t> payload;
    if (asset.ct == 1)
    {
        std::vector<uint8_t> compressed(payload_src, payload_src + payload_len);
        if (vfs_store_asset_inflate(asset, compressed, &payload) != 0)
            return false;
        if (payload.size() != asset.ul)
        {
            LOG_ERROR(
                "blob inflated to %zu bytes, planned %zu: %s",
                payload.size(),
                asset.ul,
                asset.blob.c_str());
            return false;
        }
    }
    else
    {
        payload.assign(payload_src, payload_src + payload_len);
    }

    if (!vfs_file_write_at(fp, asset.file_offset, metadata.data(), metadata.size()))
        return false;
    if (!payload.empty() &&
        !vfs_file_write_at(fp, asset.file_offset + asset.ml, payload.data(), payload.size()))
        return false;

    UopFileEntry patch;
    patch.Offset = asset.file_offset;
    patch.MetadataSize = asset.ml;
    patch.CompressedSize = uint32_t(asset.ul);
    patch.DecompressedSize = uint32_t(asset.ul);
    patch.Hash = asset.uop_hash;
    patch.Checksum = asset.mc;
    // the materialized block stores its payload raw - except BWT-encoded
    // blocks (Flags == 3), whose payload must keep the encoding the decoder
    // dispatches on
    patch.Flags = uint16_t(asset.ct == 1 ? 0 : asset.ct);
    if (!vfs_file_write_at(fp, vfs_store_table_pos(asset_index), &patch, sizeof(patch)))
        return false;
    return fflush(fp) == 0;
}

// web pack assembly: the arena backend never puts pack bytes on disk (MEMFS
// files live in the heap), but the game's file manager opens packs as files
// and reads blocks through baked pointers. This writes the assembled pack -
// the native layout, one granule resident at a time.
bool vfs_store_materialize_pack(
    class vfs_store_arena *arena,
    const VfsPlan &plan,
    size_t file_index,
    const fs_path &install_dir,
    const fs_path &blobs_dir)
{
    if (arena == nullptr || file_index >= plan.files.size())
    return false;
    const auto &file = plan.files[file_index];
    if (file.assets.empty())
    return false;

    // loose whole-file packs (the product ships e.g. gumpart/sound/MainMisc
    // as one blob each): the materialized file IS the inflated payload
    if (!file.is_uop)
    {
    const auto &asset = file.assets.front();
    if (vfs_asset_is_hole(asset))
        return false;
    const auto loose_path = fs_path_join(install_dir, file.name);
    uint64_t existing = 0;
    if (FILE *probe = fopen(fs_path_ascii(loose_path), "rb"))
    {
        vfs_file_size(probe, &existing);
        fclose(probe);
    }
    if (existing == asset.ul && asset.ul != 0)
    {
        LOG_INFO(
            "pack materialized: %s (already present, %" PRIu64 " bytes)",
            file.name.c_str(),
            existing);
        return true;
    }
    std::vector<uint8_t> payload;
    if (vfs_store_asset_payload(blobs_dir, asset, &payload) != 0)
    {
        // blob not in the store: pull it through the arena (fetches on
        // demand), then copy the granule out
        if (!arena->ensure(asset, true))
            return false;
        payload.resize(size_t(asset.ul), 0);
        if (asset.ul != 0 && !arena->read(asset, 0, payload.data(), payload.size()))
            return false;
    }
    if (!fs_file_write(fs_path_ascii(loose_path), payload.data(), payload.size()))
    {
        LOG_ERROR(
            "pack materialize: %s write failed (%zu bytes)",
            file.name.c_str(),
            payload.size());
        return false;
    }
    LOG_INFO(
        "pack materialized: %s (%zu bytes, loose whole file)",
        file.name.c_str(),
        payload.size());
    return true;
    }

    const auto path = fs_path_join(install_dir, file.name);
    bool created = false;
    FILE *fp = vfs_store_pack_open(path, file, &created);
    if (fp == nullptr)
    return false;
    if (!created)
    {
    fclose(fp); // fingerprint + size match the plan: already assembled
    return true;
    }

    LOG_INFO(
    "materializing pack: %s (%" PRIu64 " bytes, %zu assets)",
    file.name.c_str(),
    file.size,
    file.assets.size());
    bool ok = true;
    size_t written = 0;
    for (size_t ai = 0; ai < file.assets.size() && ok; ++ai)
    {
    const auto &asset = file.assets[ai];
    if (vfs_asset_is_hole(asset))
        continue; // reserved slot: the table entry stays zero (Offset == 0)
    if (!arena->ensure(asset, true))
    {
        LOG_ERROR(
            "pack materialize: %s asset %zu failed to ensure (%s)",
            file.name.c_str(),
            ai,
            asset.blob.c_str());
        ok = false;
        break;
    }
    std::vector<uint8_t> payload(size_t(asset.ul), 0);
    if (asset.ul != 0 && !arena->read(asset, 0, payload.data(), payload.size()))
    {
        LOG_ERROR(
            "pack materialize: %s asset %zu not resident after ensure",
            file.name.c_str(),
            ai);
        ok = false;
        break;
    }
    std::vector<uint8_t> metadata;
    vfs_store_synthesize_metadata(asset, metadata);
    if (!vfs_file_write_at(fp, asset.file_offset, metadata.data(), metadata.size()) ||
        (!payload.empty() &&
         !vfs_file_write_at(
             fp, asset.file_offset + asset.ml, payload.data(), payload.size())))
    {
        LOG_ERROR(
            "pack materialize: %s asset %zu write failed", file.name.c_str(), ai);
        ok = false;
        break;
    }
    UopFileEntry patch;
    patch.Offset = asset.file_offset;
    patch.MetadataSize = asset.ml;
    patch.CompressedSize = uint32_t(asset.ul);
    patch.DecompressedSize = uint32_t(asset.ul);
    patch.Hash = asset.uop_hash;
    patch.Checksum = asset.mc;
    patch.Flags = uint16_t(asset.ct == 1 ? 0 : asset.ct);
    if (!vfs_file_write_at(fp, vfs_store_table_pos(ai), &patch, sizeof(patch)))
    {
        LOG_ERROR(
            "pack materialize: %s asset %zu table patch failed",
            file.name.c_str(),
            ai);
        ok = false;
        break;
    }
    ++written;
    }
    const bool flushed = fflush(fp) == 0;
    fclose(fp);
    if (!ok || !flushed)
    {
    LOG_ERROR(
        "pack materialize failed: %s (%zu/%zu assets, flush %d)",
        file.name.c_str(),
        written,
        file.assets.size(),
        int(flushed));
    return false;
    }
    LOG_INFO(
    "pack materialized: %s (%zu assets)", file.name.c_str(), file.assets.size());
    return true;
}

// arena-free counterpart of vfs_store_materialize_pack: the payload comes
// from the blob store (vfs_store_asset_payload), never from a resident
// granule. Web boot uses this - the arena holds decompressed payloads in the
// wasm heap, and a full-plan residency cannot fit next to the install files.
// Every asset is written in the pack's native sparse layout; an asset whose
// blob is not in the store keeps its zeroed slot (the caller censuses it).
vfs_pack_write_stats vfs_store_materialize_pack_from_store(
    const VfsPlan &plan,
    size_t file_index,
    const fs_path &install_dir,
    const fs_path &blobs_dir)
{
    vfs_pack_write_stats stats = {};
    if (file_index >= plan.files.size())
        return stats;
    const auto &file = plan.files[file_index];
    if (file.assets.empty())
        return stats;

    // loose whole-file packs: the materialized file IS the inflated payload
    if (!file.is_uop)
    {
        const auto &asset = file.assets.front();
        if (vfs_asset_is_hole(asset))
            return stats;
        std::vector<uint8_t> payload;
        const int prv = vfs_store_asset_payload(blobs_dir, asset, &payload);
        if (prv != 0)
        {
            LOG_WARN(
                "web pack %s: blob payload unavailable (%s, res %d) - stays empty",
                file.name.c_str(),
                asset.blob.c_str(),
                prv);
            stats.failed = 1;
            return stats;
        }
        if (!fs_file_write(
                fs_path_ascii(fs_path_join(install_dir, file.name)),
                payload.data(),
                payload.size()))
        {
            LOG_ERROR("web pack %s: write failed (%zu bytes)", file.name.c_str(), payload.size());
            stats.failed = 1;
            return stats;
        }
        stats.written = 1;
        stats.bytes = payload.size();
        return stats;
    }

    const auto path = fs_path_join(install_dir, file.name);
    bool created = false;
    FILE *fp = vfs_store_pack_open(path, file, &created);
    if (fp == nullptr)
    {
        LOG_ERROR(
            "web pack %s: open/create failed (%s, %llu bytes planned)",
            file.name.c_str(),
            fs_path_ascii(path),
            (unsigned long long)file.size);
        stats.failed = 1;
        return stats;
    }
    if (!created)
    {
        // fingerprint + size match the plan: already assembled. The caller
        // re-censuses stubs, so a warm boot still reports its gaps.
        fclose(fp);
        stats.skipped = 1;
        return stats;
    }

    for (size_t ai = 0; ai < file.assets.size(); ++ai)
    {
        const auto &asset = file.assets[ai];
        if (vfs_asset_is_hole(asset))
            continue; // reserved slot: the table entry stays zero

        std::vector<uint8_t> payload;
        const int prav = vfs_store_asset_payload(blobs_dir, asset, &payload);
        if (prav != 0)
        {
            ++stats.stubbed; // zero slot + unpatched table entry
            if (stats.stubbed <= 3)
            {
                LOG_WARN(
                    "web pack %s: asset %zu blob unavailable (res %d, %s)",
                    file.name.c_str(),
                    ai,
                    prav,
                    asset.blob.c_str());
            }
            continue;
        }
        std::vector<uint8_t> metadata;
        vfs_store_synthesize_metadata(asset, metadata);
        if (!vfs_file_write_at(fp, asset.file_offset, metadata.data(), metadata.size()) ||
            (!payload.empty() &&
             !vfs_file_write_at(
                 fp, asset.file_offset + asset.ml, payload.data(), payload.size())))
        {
            ++stats.failed;
            continue;
        }
        UopFileEntry patch;
        patch.Offset = asset.file_offset;
        patch.MetadataSize = asset.ml;
        patch.CompressedSize = uint32_t(asset.ul);
        patch.DecompressedSize = uint32_t(asset.ul);
        patch.Hash = asset.uop_hash;
        patch.Checksum = asset.mc;
        patch.Flags = uint16_t(asset.ct == 1 ? 0 : asset.ct);
        if (!vfs_file_write_at(fp, vfs_store_table_pos(ai), &patch, sizeof(patch)))
        {
            ++stats.failed;
            continue;
        }
        ++stats.written;
        stats.bytes += payload.size();
    }
    const bool flushed = fflush(fp) == 0;
    fclose(fp);
    if (!flushed)
        ++stats.failed;
    return stats;
}

// loose files: materialized whole from the blob (decompressed when ct == 1)
// ---------------------------------------------------------------------------

// pure payload step: stored assets pass through, zlib assets inflate -
// the exact bytes the loose writer has always put on disk, now shared with
// the arena backend (no duplicated format knowledge)
int vfs_store_asset_inflate(
    const VfsAsset &asset, const std::vector<uint8_t> &blob, std::vector<uint8_t> *out)
{
    const uint8_t *data = blob.data();
    size_t size = blob.size();
    if (asset.ct == 1)
    {
        out->resize(asset.ul);
        uLongf dst_len = out->size();
        auto *src = reinterpret_cast<Bytef const *>(blob.data());
        if (uncompress(out->data(), &dst_len, src, uLong(blob.size())) != Z_OK)
        {
            LOG_ERROR("blob failed to decompress: %s", asset.blob.c_str());
            return 2;
        }
        out->resize(size_t(dst_len));
        return 0;
    }
    out->assign(data, data + size);
    return 0;
}

// blob read + inflate in one step (the arena's ensure path)
int vfs_store_asset_payload(
    const fs_path &blobs_dir, const VfsAsset &asset, std::vector<uint8_t> *out)
{
    std::vector<uint8_t> blob;
    const int res = vfs_store_read_blob(blobs_dir, asset, blob);
    if (res != 0)
        return res;
    return vfs_store_asset_inflate(asset, blob, out);
}

static bool vfs_store_write_loose(
    const vfs_store_params &params, const VfsAsset &asset, const std::vector<uint8_t> &blob)
{
    const auto path = fs_path_join(params.install_dir, asset.name);
    fs_path_create(fs_directory(path));

    std::vector<uint8_t> payload;
    if (vfs_store_asset_inflate(asset, blob, &payload) != 0)
        return false;

    const char *ascii = fs_path_ascii(path);
    // a managed file may hold a trap mapping: windows refuses to truncate a
    // file with a mapped view (ERROR_USER_MAPPED_FILE), so write in place and
    // only fall back to create/truncate when the file does not exist yet
    FILE *fp = fopen(ascii, "r+b");
    if (!fp)
        fp = fopen(ascii, "wb");
    if (!fp)
        return false;
    const bool wrote =
        fseek(fp, 0, SEEK_SET) == 0 && fwrite(payload.data(), 1, payload.size(), fp) == payload.size();
    if (wrote)
    {
        // best-effort size match (a mapped sparse file is already the exact
        // plan size and windows refuses to resize it - ignore that failure)
        fflush(fp);
#if defined(XUO_WINDOWS)
        _chsize_s(_fileno(fp), static_cast<long long>(payload.size()));
        // publish the buffered writes to mapped views of the same file
        _commit(_fileno(fp));
#else
        if (ftruncate(fileno(fp), static_cast<off_t>(payload.size())) != 0)
        {
            /* mapped on posix: length already matches, ignore */
        }
        fsync(fileno(fp));
#endif
    }
    fclose(fp);
    return wrote;
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
    std::vector<uint8_t> file_dirty;        // per file: written to this run
    const std::vector<uint8_t> *provided_blob = nullptr; // caller-held source bytes
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
    {
        // rate limit: a closed/failed journal once meant ~12k identical lines
        // per boot (the seal marks every complete file on every run)
        static std::atomic<uint32_t> s_suppressed{ 0 };
        const uint32_t n = s_suppressed.fetch_add(1) + 1;
        if (n <= 3 || (n % 100) == 0)
            LOG_WARN("journal mark failed: %c %u %u (x%u)", op, fi, ai, n);
    }
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
    if (ctx.provided_blob != nullptr)
    {
        blob = *ctx.provided_blob; // the trap's range fetch delivered it
    }
    else
    {
        const int res = vfs_store_read_blob(ctx.params.blobs_dir, asset, blob);
        if (res == 1)
        {
            vfs_store_count(ctx, &vfs_materialize_stats::assets_missing);
            return false; // stays pending: fetch-engine work list
        }
        if (res != 0)
        {
            (*ctx.states)[gi] = vfs_failed;
            vfs_store_count(ctx, &vfs_materialize_stats::assets_failed);
            return false;
        }
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
    if (file_index < ctx.file_dirty.size())
        ctx.file_dirty[file_index] = 1;
    vfs_store_mark(ctx, 'F', uint32_t(file_index), uint32_t(asset_index));
    vfs_store_count(ctx, &vfs_materialize_stats::assets_fetched);
    if (ctx.stats)
        ctx.stats->bytes_written += vfs_asset_block_size(asset);
    // asset recorder: the pending -> fetched transition of a run the game
    // waited on (trap resolution, blocking tier); loose files carry their
    // name in the pack column (there is no pack). Prefetch and convergence
    // runs are cache-warming, not usage, and must not seed the T1 manifest
    if (ctx.params.record)
    {
        vfs_record_asset(
            asset.uop_hash,
            vfs_asset_is_loose(asset) ? asset.name : asset.uop_file,
            uint64_t(asset.ul),
            asset.tier);
    }
    return true;
}

// seals files that reached 100% of their expected (non-hole) assets this
// run. Only files written to by this run are considered: a sealed file never
// changes bytes again, and re-sealing every complete file on every (single
// asset) run used to cost O(plan) scans + a journal mark + a full-pack fsync
// per call. Crash durability of the pack bytes themselves rides on the disk
// truth (reconcile) + the state folds, not on a flush here.
static void vfs_store_seal(vfs_store_context &ctx)
{
    for (size_t fi = 0; fi < ctx.plan->files.size(); ++fi)
    {
        if (fi >= ctx.file_dirty.size() || ctx.file_dirty[fi] == 0)
            continue;
        const auto &file = ctx.plan->files[fi];
        const size_t base = vfs_store_global_index(*ctx.plan, fi, 0);
        bool complete = !file.assets.empty();
        for (size_t ai = 0; ai < file.assets.size(); ++ai)
        {
            if (!vfs_asset_is_hole(file.assets[ai]) &&
                (*ctx.states)[base + ai] != vfs_fetched)
                complete = false;
        }
        if (complete)
            vfs_store_mark(ctx, 'S', uint32_t(fi), 0);
    }
}

// total planned bytes of a plan (pack tables + blocks + loose payloads)
static uint64_t vfs_store_planned_bytes(const VfsPlan &plan)
{
    uint64_t total = 0;
    for (const auto &file : plan.files)
        total += file.size;
    return total;
}

bool vfs_store_materialize(
    const VfsPlan &plan,
    const vfs_store_params &params,
    std::vector<uint8_t> &states,
    vfs_journal *journal,
    vfs_materialize_stats *stats,
    const std::vector<vfs_store_item> *items)
{
    vfs_materialize_stats run;
    vfs_store_context ctx;
    ctx.plan = &plan;
    ctx.params = params;
    ctx.states = &states;
    ctx.journal = journal;
    ctx.stats = &run;
    ctx.file_dirty.assign(plan.files.size(), 0);
    const auto run_start = std::chrono::steady_clock::now();

    vfs_store_check_sparse(vfs_store_planned_bytes(plan), params.install_dir);

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
        // a pack not on disk yet is just the streaming install being built
        // up - only an EXISTING file being thrown away is worth announcing
        // (this runs on every materialize run while the install is
        // incomplete, so a blanket log repeats the whole list every chunk)
        if (fs_path_exists(path))
            LOG_INFO("recreating pack for plan: %s", file.name.c_str());
    }

    fs_path_create(params.install_dir);

    if (items != nullptr)
    {
        // fetch-driven subset materialization (boot): the exact same write
        // path, restricted to the assets the caller fetched for
        for (const auto &item : *items)
        {
            if (item.file_index >= plan.files.size())
                continue;
            const auto &file = plan.files[item.file_index];
            if (item.asset_index >= file.assets.size())
                continue;
            ++run.assets_total;
            vfs_store_materialize_asset(ctx, item.file_index, item.asset_index);
        }
    }
    else
    {
        for (size_t fi = 0; fi < plan.files.size(); ++fi)
        {
            for (size_t ai = 0; ai < plan.files[fi].assets.size(); ++ai)
            {
                ++run.assets_total;
                vfs_store_materialize_asset(ctx, fi, ai);
            }
        }
    }
    vfs_store_seal(ctx);

    if (stats)
        *stats = run;
    const double run_secs =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - run_start).count();
    if (!params.quiet)
        LOG_INFO(
            "materialize: %zu assets (%zu written, %zu present, %zu missing, %zu failed, %" PRIu64
            " bytes) in %.3fs",
            run.assets_total,
            run.assets_fetched,
            run.assets_present,
            run.assets_missing,
            run.assets_failed,
            run.bytes_written,
            run_secs);
    return run.assets_failed == 0;
}

// ---------------------------------------------------------------------------
// boot skeleton: pack headers + full entry tables (blocks stay holes) plus
// right-sized sparse loose files - everything Load() needs, no payload yet
// ---------------------------------------------------------------------------

// sparse body at the planned size for a loose file; content fills later
static bool vfs_store_loose_prepare(const vfs_store_params &params, const VfsAsset &asset)
{
    const auto path = fs_path_join(params.install_dir, asset.name);
    if (fs_path_exists(path))
    {
        // already the right size (complete or sparse): leave it alone
        if (FILE *probe = fopen(fs_path_ascii(path), "rb"))
        {
            uint64_t existing = 0;
            const bool sized = vfs_file_size(probe, &existing);
            fclose(probe);
            if (sized && existing == asset.ul)
                return true;
        }
    }
    fs_path_create(fs_directory(path));
    FILE *fp = fopen(fs_path_ascii(path), "wb");
    if (!fp)
    {
        LOG_ERROR("could not create loose file: %s", asset.name.c_str());
        return false;
    }
    const bool ok = vfs_file_truncate(fp, asset.ul);
    fclose(fp);
    if (!ok)
        LOG_ERROR("could not size loose file: %s", asset.name.c_str());
    return ok;
}

// creates every managed file's boot skeleton without fetching a byte: pack
// headers + section + entry tables (all blocks still holes) and loose files
// sized to their planned size (sparse). This is what makes g_FileManager
// find valid, correctly sized files right after the blocking tier.
bool vfs_store_prepare(const VfsPlan &plan, const vfs_store_params &params)
{
    fs_path_create(params.install_dir);
    vfs_store_check_sparse(vfs_store_planned_bytes(plan), params.install_dir);
    bool ok = true;
    for (const auto &file : plan.files)
    {
        const auto path = fs_path_join(params.install_dir, file.name);
        if (file.is_uop)
        {
            bool created = false;
            FILE *fp = vfs_store_pack_open(path, file, &created);
            if (fp)
            {
                if (created)
                    LOG_TRACE("prepared pack: %s", file.name.c_str());
                fclose(fp);
            }
            else
            {
                ok = false;
            }
        }
        else if (!file.assets.empty())
        {
            ok = vfs_store_loose_prepare(params, file.assets[0]) && ok;
        }
    }
    return ok;
}

// ---------------------------------------------------------------------------
// update flow: invalidate one asset's materialized bytes
// ---------------------------------------------------------------------------

bool vfs_store_invalidate_asset(
    const VfsPlan &plan,
    const vfs_store_params &params,
    uint32_t file_index,
    uint32_t asset_index)
{
    if (file_index >= plan.files.size())
        return false;
    const auto &file = plan.files[file_index];
    if (asset_index >= file.assets.size())
        return false;
    const auto &asset = file.assets[asset_index];
    if (vfs_asset_is_hole(asset))
        return true; // nothing materialized, nothing to invalidate

    if (!file.is_uop)
    {
        // loose files have no partial on-disk state: the whole payload is the
        // file, so it goes away entirely (re-skeletonized by prepare/write)
        const auto path = fs_path_join(params.install_dir, asset.name);
        fs_del(path);
        return !fs_path_exists(path);
    }

    // stored layout: every materialized entry advertises the plan's ul bytes
    // with Flags == 0; the hole re-opens (Offset = 0) and UopLoadFile skips
    // the entry again until the block is rewritten
    UopFileEntry entry;
    entry.Offset = 0;
    entry.MetadataSize = asset.ml;
    entry.CompressedSize = uint32_t(asset.ul);
    entry.DecompressedSize = uint32_t(asset.ul);
    entry.Hash = asset.uop_hash;
    entry.Checksum = asset.mc;
    entry.Flags = 0;
    const auto path = fs_path_join(params.install_dir, file.name);
    FILE *fp = fopen(fs_path_ascii(path), "r+b");
    if (!fp)
        return !fs_path_exists(path); // a missing file is already invalid
    const bool ok =
        vfs_file_write_at(fp, vfs_store_table_pos(asset_index), &entry, sizeof(entry)) &&
        fflush(fp) == 0;
    fclose(fp);
    if (!ok)
        LOG_ERROR("could not invalidate block %u of %s", asset_index, file.name.c_str());
    return ok;
}

// ---------------------------------------------------------------------------
// verification
// ---------------------------------------------------------------------------

// head bytes compared between the on-disk stored payload and the blob's
// decompressed head; separates a real block from a stub patch's zeros
#define VFS_PACK_HEAD_BYTES 64

// inflates just the head bytes of the asset's zlib blob; false when the blob
// cannot supply the planned head (missing, truncated, zlib failure)
static bool vfs_store_blob_payload_head(
    const fs_path &blobs_dir, const VfsAsset &asset, size_t want, uint8_t *out_head)
{
    std::vector<uint8_t> blob;
    if (vfs_store_read_blob(blobs_dir, asset, blob) != 0)
        return false;
    // range assets carry [ml metadata][payload]: the payload starts past ml
    const size_t skip = asset.blob_offset != 0 ? size_t(asset.ml) : 0;
    if (blob.size() < skip)
        return false;
    const uint8_t *data = blob.data() + skip;
    const size_t size = blob.size() - skip;
    if (asset.ct == 0)
    {
        if (size < want)
            return false;
        memcpy(out_head, data, want);
        return true;
    }
    std::vector<uint8_t> head(want, 0);
    z_stream zs = {};
    if (inflateInit(&zs) != Z_OK)
        return false;
    zs.next_in = const_cast<Bytef *>(data);
    zs.avail_in = uint32_t(size);
    zs.next_out = head.data();
    zs.avail_out = uint32_t(want);
    const int res = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    if (res != Z_STREAM_END && res != Z_OK && res != Z_BUF_ERROR)
        return false;
    if (zs.total_out < want)
        return false;
    memcpy(out_head, head.data(), want);
    return true;
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
    bool stubbed)
{
    if (vfs_asset_is_hole(asset))
        return vfs_verify_ok;
    // the state DB is the stub truth: a placeholder patch reads as a
    // missing block (never corrupt) and the file stays unsealed
    if (stubbed)
        return vfs_verify_missing_block;

    if (!start || file_size < vfs_store_table_pos(asset_index) + sizeof(UopFileEntry))
        return vfs_store_blob_exists(params, asset) ? vfs_verify_missing_block :
                                                      vfs_verify_missing_blob;

    UopFileEntry entry;
    memcpy(&entry, start + vfs_store_table_pos(asset_index), sizeof(entry));
    if (entry.Offset == 0) // hole in the materialized file
        return vfs_store_blob_exists(params, asset) ? vfs_verify_missing_block :
                                                      vfs_verify_missing_blob;

    // stored layout: the entry must carry the plan's shape with Flags == 0
    const uint16_t flags = 0;
    if (entry.Offset != asset.file_offset || entry.MetadataSize != asset.ml ||
        entry.CompressedSize != asset.ul || entry.DecompressedSize != asset.ul ||
        entry.Hash != asset.uop_hash || entry.Checksum != asset.mc || entry.Flags != flags)
        return vfs_verify_corrupt;
    if (entry.Offset + entry.MetadataSize + entry.CompressedSize > file_size)
        return vfs_verify_corrupt;
    if (!entry.CompressedSize)
        return vfs_verify_corrupt;

    // shape decided; the blob (when available) confirms the content: the
    // stored payload must start with the blob's decompressed head - a stub
    // patch's placeholder bytes never do
    const size_t head = size_t(asset.ul < VFS_PACK_HEAD_BYTES ? asset.ul : VFS_PACK_HEAD_BYTES);
    if (head == 0)
        return vfs_verify_ok;
    uint8_t blob_head[VFS_PACK_HEAD_BYTES] = {};
    if (!vfs_store_blob_payload_head(params.blobs_dir, asset, head, blob_head))
    {
        // without a usable blob the shape is the only verdict (same contract
        // as the loose verifier's size-only fallback); missing blobs are the
        // norm mid-stream, so this stays quiet
        LOG_TRACE("verify: unusable blob for %s, shape check only", asset.blob.c_str());
        return vfs_verify_ok;
    }
    const uint8_t *stored = start + entry.Offset + entry.MetadataSize;
    return memcmp(stored, blob_head, head) == 0 ? vfs_verify_ok : vfs_verify_corrupt;
}

static uint8_t vfs_verify_loose_asset(
    const vfs_store_params &params,
    const VfsAsset &asset,
    bool stubbed,
    std::vector<uint8_t> &scratch)
{
    (void)scratch;
    if (vfs_asset_is_hole(asset))
        return vfs_verify_ok;
    if (stubbed) // the whole-file placeholder serve (zeros) - not corrupt
        return vfs_verify_missing_block;

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

// on-disk truth check shared with the state DB reconcile and the trap's boot
// presence: the block bytes of `asset` are present in `file` exactly as
// planned. Hardening: a stub patch must never read as the real thing. Every
// materialized entry carries the same stored shape (Flags == 0,
// CompressedSize == ul), so the entry shape alone cannot separate a stub
// from a real block - with `blobs_dir`, the reconcile-grade strictness opts
// into a content check: the stored head is compared against the blob's
// decompressed head, and a missing blob counts as not-present. The
// invariant: stub entries are never promoted to fetched by reconcile -
// only the heal path (the materialize write + its journal 'F') promotes.
bool vfs_store_pack_asset_present(
    const uint8_t *start,
    uint64_t file_size,
    size_t asset_index,
    const VfsAsset &asset,
    const fs_path &blobs_dir)
{
    if (!start || file_size < vfs_store_table_pos(asset_index) + sizeof(UopFileEntry))
        return false;
    if (vfs_asset_is_hole(asset))
        return true; // holes are satisfied by the reserved table slot
    UopFileEntry entry;
    memcpy(&entry, start + vfs_store_table_pos(asset_index), sizeof(entry));
    const bool shape = entry.Offset == asset.file_offset && entry.Offset != 0 &&
                       entry.MetadataSize == asset.ml && entry.CompressedSize == asset.ul &&
                       entry.DecompressedSize == asset.ul && entry.Hash == asset.uop_hash &&
                       entry.Flags == uint16_t(0) && entry.Checksum == asset.mc &&
                       entry.Offset + entry.MetadataSize + entry.CompressedSize <= file_size;
    if (!shape)
        return false;
    if (fs_path_empty(blobs_dir))
        return true; // no store root: the entry shape decided
    // content decides. The blob's decompressed head versus the stored head
    // separates a real block from the zeros/header tail of a stub patch; a
    // missing blob cannot prove realness
    const size_t head = size_t(asset.ul < VFS_PACK_HEAD_BYTES ? asset.ul : VFS_PACK_HEAD_BYTES);
    if (head == 0)
        return true;
    uint8_t blob_head[VFS_PACK_HEAD_BYTES] = {};
    if (!vfs_store_blob_payload_head(blobs_dir, asset, head, blob_head))
        return false;
    return memcmp(start + entry.Offset + entry.MetadataSize, blob_head, head) == 0;
}

// head bytes compared between the on-disk payload and the (decompressed)
// blob; full content verification is verify's job, this is the cheap
// reconcile-grade check (shared constant with the pack path)
//
// The check decides on the HEAD window first and only walks the deeper
// windows when the head is genuinely ambiguous (both sides all zero - e.g.
// a unifont payload vs the sparse skeleton). Reconcile runs over EVERY asset
// at EVERY boot on the boot thread, so this is also what keeps the common
// cases from decoding a whole zlib blob just to say "not the skeleton":
// head differs -> not proven (false), head equal and nonzero -> proven
// (true), head equal and all-zero -> keep sampling.
bool vfs_store_loose_asset_content_ok(
    const fs_path &path, const VfsAsset &asset, const fs_path &blobs_dir)
{
    if (fs_path_empty(blobs_dir) || vfs_asset_is_hole(asset))
        return true; // no store root: presence cannot be content-checked
    if (asset.ul == 0)
        return true;

    // sample windows: head, and for larger files quarter/middle/three-quarter
    // - compared against the source payload. The multi-window compare exists
    // because several real assets (every unifont*.mul) legitimately start
    // with a long zero region: a head-only compare cannot separate their
    // never-materialized zero skeleton from the real downloaded payload.
    struct vfs_content_sample
    {
        uint64_t offset = 0;
        uint32_t length = 0;
        bool captured = false;
        uint8_t disk[0x1000] = {};
        uint8_t want[0x1000] = {};
    };
    const uint64_t size = asset.ul;
    vfs_content_sample samples[4];
    size_t sample_count = 0;
    // decides from the head window alone when it can: 0 = keep sampling,
    // 1 = proven present, -1 = proven absent
    const auto head_verdict = [&samples]() {
        const auto &head = samples[0];
        if (memcmp(head.disk, head.want, head.length) != 0)
            return -1;
        for (uint32_t i = 0; i < head.length; ++i)
        {
            if (head.disk[i] != 0)
                return 1; // real nonzero payload in place: not the skeleton
        }
        return 0; // both heads all zero: ambiguous (zero-head payload vs skeleton)
    };
    const auto add_sample = [&](uint64_t offset)
    {
        if (offset == UINT64_MAX || offset >= size || sample_count == 4)
            return;
        vfs_content_sample &sample = samples[sample_count++];
        sample.offset = offset;
        sample.length = uint32_t(offset + 0x1000 <= size ? 0x1000 : size - offset);
    };
    add_sample(0);
    if (size > 0x4000)
        add_sample(size / 4);
    if (size > 0x8000)
        add_sample(size / 2);
    if (size > 0xC000)
        add_sample(size - 0x1000);
    if (sample_count == 0)
        return true;

    // read the file's windows
    FILE *fp = fopen(fs_path_ascii(path), "rb");
    if (!fp)
        return false;
    for (size_t si = 0; si < sample_count; ++si)
    {
        auto &sample = samples[si];
        if (fseeko(fp, off_t(sample.offset), SEEK_SET) != 0 ||
            fread(sample.disk, 1, sample.length, fp) != sample.length)
        {
            fclose(fp);
            return false; // short read: the file is not the real payload
        }
    }
    fclose(fp);

    const auto wire_path = fs_path_join(blobs_dir, asset.blob);
    FILE *wire = fopen(fs_path_ascii(wire_path), "rb");
    if (!wire)
        return false; // a missing blob never proves presence

    bool ok = true;
    if (asset.ct == 0)
    {
        // stored: the wire IS the payload - windows compare directly, but the
        // head window decides when it can (see head_verdict)
        for (size_t si = 0; si < sample_count; ++si)
        {
            auto &sample = samples[si];
            if (fseeko(wire, off_t(sample.offset), SEEK_SET) != 0 ||
                fread(sample.want, 1, sample.length, wire) != sample.length)
            {
                ok = false;
                break;
            }
            if (memcmp(sample.disk, sample.want, sample.length) != 0)
            {
                ok = false;
                break;
            }
            if (si == 0)
            {
                const int verdict = head_verdict();
                if (verdict != 0)
                {
                    ok = verdict == 1;
                    break;
                }
            }
        }
    }
    else
    {
        // zlib: one progressive pass, capturing the sample windows as the
        // stream produces them. The head window decides as soon as it is
        // captured (head differs -> not the payload; head equal and nonzero
        // -> proven) so the common cases stop after ~64KB of inflate instead
        // of decoding the whole blob; only the all-zero ambiguous head keeps
        // inflating toward the deeper windows
        std::vector<uint8_t> wire_data;
        {
            uint8_t buf[0x10000];
            size_t got = 0;
            while ((got = fread(buf, 1, sizeof(buf), wire)) > 0)
                wire_data.insert(wire_data.end(), buf, buf + got);
        }
        z_stream zs = {};
        if (inflateInit(&zs) != Z_OK)
        {
            ok = false;
        }
        else
        {
            uint64_t produced = 0;
            size_t windows_left = sample_count;
            bool decided = false;
            zs.next_in = reinterpret_cast<Bytef *>(wire_data.data());
            zs.avail_in = uint32_t(wire_data.size());
            uint8_t out[0x10000];
            int res = Z_OK;
            while (windows_left != 0 && res != Z_STREAM_END && !decided)
            {
                zs.next_out = out;
                zs.avail_out = sizeof(out);
                const uint64_t before = zs.total_out;
                res = inflate(&zs, Z_NO_FLUSH);
                const uint64_t produced_now = zs.total_out - before;
                if (produced_now != 0)
                {
                    const uint64_t chunk_start = produced;
                    for (size_t si = 0; si < sample_count; ++si)
                    {
                        auto &sample = samples[si];
                        const uint64_t s = sample.offset;
                        const uint64_t e = s + sample.length;
                        if (s >= chunk_start && e <= chunk_start + produced_now &&
                            !sample.captured)
                        {
                            memcpy(sample.want, out + size_t(s - chunk_start), sample.length);
                            sample.captured = true;
                            --windows_left;
                            if (si == 0)
                            {
                                const int verdict = head_verdict();
                                if (verdict == 1)
                                {
                                    ok = true;
                                    decided = true;
                                }
                                else if (verdict == -1)
                                {
                                    ok = false;
                                    decided = true;
                                }
                                // 0: ambiguous zero head - keep sampling
                            }
                        }
                    }
                    produced = chunk_start + produced_now;
                }
                if (zs.avail_in == 0 && windows_left != 0 && !decided)
                    break; // input exhausted before all windows were captured
            }
            inflateEnd(&zs);
            if (!decided)
            {
                for (size_t si = 0; si < sample_count; ++si)
                {
                    auto &sample = samples[si];
                    if (!sample.captured)
                    {
                        ok = false; // the source cannot supply the sample: unproven
                        break;
                    }
                    if (memcmp(sample.disk, sample.want, sample.length) != 0)
                    {
                        ok = false;
                        break;
                    }
                }
            }
        }
    }
    fclose(wire);
    return ok;
}
// ---------------------------------------------------------------------------
// the native backend: store_sparse
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
        (void)block; // always materializes synchronously
        size_t fi = 0, ai = 0;
        if (!locate(asset, fi, ai))
            return false;
        vfs_store_context ctx = context();
        return vfs_store_materialize_asset(ctx, fi, ai);
    }

    void prefetch(const VfsAsset &asset, int prio) override
    {
        (void)asset;
        (void)prio; // the fetch engine queues the blob download
    }

    void evict(size_t bytes_target) override
    {
        (void)bytes_target; // web arena only; disk files are never evicted
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
        ctx.file_dirty.assign(m_plan->files.size(), 0);
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

vfs_store_backend *vfs_store_backend_create(
    vfs_store_backend_kind kind, const vfs_store_params &params)
{
    switch (kind)
    {
        case vfs_store_backend_arena:
        {
            vfs_store_arena_params arena_params;
            arena_params.store = params;
            return vfs_store_backend_arena_create(arena_params);
        }
        case vfs_store_backend_sparse:
        default:
            return vfs_store_backend_sparse_create(params);
    }
}

// ---------------------------------------------------------------------------
// trap support
// ---------------------------------------------------------------------------

// page-coverage computation over the plan's deterministic layout: the block
// indices whose [file_offset, file_offset + ml + payload) range overlaps
// [start, end) - exactly the bytes the write path puts down per asset
void vfs_store_blocks_overlapping(
    const VfsPlan::VfsFile &file,
    uint64_t start,
    uint64_t end,
    std::vector<uint32_t> *out)
{
    if (!out)
        return;
    for (size_t ai = 0; ai < file.assets.size(); ++ai)
    {
        const auto &asset = file.assets[ai];
        if (vfs_asset_is_hole(asset))
            continue; // reserved slot, no bytes, no coverage
        const uint64_t block_start = asset.file_offset;
        const uint64_t block_end = block_start + vfs_asset_block_size(asset);
        if (block_end <= start)
            continue; // fully before the range (planned layout is ascending)
        if (block_start >= end)
            break; // fully after the range: nothing further can overlap
        out->push_back(uint32_t(ai));
    }
}

// one-asset materialization through the shared write path (the same code as
// the batch driver, restricted to a single item). When `provided` is set it
// IS the source bytes (the trap's range fetch); otherwise the blob store is
// read. Returns true only when the asset bytes are confirmed in place
// afterwards (state == fetched), so a missing blob is reported as failure,
// not as a clean no-op run.
static bool vfs_store_rematerialize_asset_impl(
    const VfsPlan &plan,
    const vfs_store_params &params,
    std::vector<uint8_t> &states,
    vfs_journal *journal,
    uint32_t file_index,
    uint32_t asset_index,
    const std::vector<uint8_t> *provided)
{
    if (file_index >= plan.files.size())
        return false;
    const auto &file = plan.files[file_index];
    if (asset_index >= file.assets.size())
        return false;
    const size_t gi = vfs_store_global_index(plan, file_index, asset_index);
    std::vector<vfs_store_item> items;
    items.push_back({ file_index, asset_index });
    vfs_materialize_stats stats;

    if (provided == nullptr)
    {
        vfs_store_context probe;
        probe.plan = &plan;
        probe.params = params;
        probe.states = &states;
        probe.journal = journal;
        std::vector<uint8_t> blob;
        if (vfs_store_read_blob(params.blobs_dir, file.assets[asset_index], blob) != 0)
            return false; // missing or unusable: the caller may retry
        if (!vfs_store_materialize(plan, params, states, journal, &stats, &items))
            return false;
        return gi < states.size() && states[gi] == vfs_fetched;
    }

    // materialize from the provided bytes through the shared single-item path:
    // stage the blob where the writer expects it. The store context reads
    // blobs per asset, so a provided run goes through a one-asset context
    // with the blob handed over (the read is skipped for a provided run)
    vfs_store_context ctx;
    ctx.plan = &plan;
    ctx.params = params;
    ctx.states = &states;
    ctx.journal = journal;
    ctx.stats = &stats;
    ctx.file_dirty.assign(plan.files.size(), 0);
    ctx.provided_blob = provided;
    if (gi < states.size())
        states[gi] = vfs_pending; // force the write path (idempotent anyway)
    const bool ok = vfs_store_materialize_asset(ctx, file_index, asset_index);
    vfs_store_seal(ctx);
    return ok && gi < states.size() && states[gi] == vfs_fetched;
}

bool vfs_store_rematerialize_asset(
    const VfsPlan &plan,
    const vfs_store_params &params,
    std::vector<uint8_t> &states,
    vfs_journal *journal,
    uint32_t file_index,
    uint32_t asset_index)
{
    return vfs_store_rematerialize_asset_impl(
        plan, params, states, journal, file_index, asset_index, nullptr);
}

bool vfs_store_materialize_asset_data(
    const VfsPlan &plan,
    const vfs_store_params &params,
    std::vector<uint8_t> &states,
    vfs_journal *journal,
    uint32_t file_index,
    uint32_t asset_index,
    const std::vector<uint8_t> &block_bytes)
{
    return vfs_store_rematerialize_asset_impl(
        plan, params, states, journal, file_index, asset_index, &block_bytes);
}
int vfs_store_verify(
    const VfsPlan &plan,
    const vfs_store_params &params,
    vfs_verify_result *out,
    const std::vector<uint8_t> *states)
{
    vfs_verify_result result;
    int corrupt = 0;
    size_t global_index = 0;
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
            const bool stubbed =
                states != nullptr && global_index < states->size() &&
                (*states)[global_index] == vfs_stubbed;
            std::vector<uint8_t> scratch;
            vfs_verify_asset va;
            va.uop_hash = asset.uop_hash;
            va.name = asset.name;
            va.stubbed = stubbed;
            va.status = file.is_uop ?
                            vfs_verify_pack_asset(
                                params, mapped, mapped_size, ai, asset, stubbed) :
                            vfs_verify_loose_asset(params, asset, stubbed, scratch);
            ++vf.counts[va.status];
            if (va.status == vfs_verify_corrupt)
            {
                ++corrupt;
                result.corrupted = true;
            }
            vf.assets.emplace_back(va);
            ++global_index;
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


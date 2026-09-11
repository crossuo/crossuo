// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

#define LOGGER_MODULE Vfs

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#if !defined(XUO_WINDOWS)
#include <unistd.h> // fsync
#endif

#include <vector>

#include "vfs_state.h"
#include "vfs_store.h"
#include "uop.h"

#include "common.h"
#include <common/logging/logging.h>

// ---------------------------------------------------------------------------
// serialization helpers (plain little-endian PODs + length-prefixed strings)
// ---------------------------------------------------------------------------

struct vfs_writer
{
    std::vector<uint8_t> data;

    template <typename T>
    void pod(const T &value)
    {
        const auto *bytes = reinterpret_cast<const uint8_t *>(&value);
        data.insert(data.end(), bytes, bytes + sizeof(T));
    }

    void str(const astr_t &text)
    {
        const uint32_t len = uint32_t(text.size());
        pod(len);
        data.insert(data.end(), text.begin(), text.end());
    }
};

struct vfs_reader
{
    const uint8_t *data = nullptr;
    size_t size = 0;
    size_t pos = 0;

    template <typename T>
    bool pod(T *value)
    {
        if (!data || pos + sizeof(T) > size)
            return false;
        memcpy(value, data + pos, sizeof(T));
        pos += sizeof(T);
        return true;
    }

    bool str(astr_t *text)
    {
        uint32_t len = 0;
        if (!pod(&len) || !data || pos + len > size)
            return false;
        text->assign(reinterpret_cast<const char *>(data) + pos, len);
        pos += len;
        return true;
    }
};

static const uint32_t VFS_PLAN_MAGIC = 0x314c5056;  // "VPL1"
static const uint32_t VFS_STATE_MAGIC = 0x31545356; // "VST1"
// 2: the plan carries the product manifest/file repo URLs (fetch engine);
// older plan.bin files simply fail to load and the plan is rebuilt
// 3: the plan records its product subset - additive: a v3
// plan is a v2 plan plus one trailing byte, and v2 plans load unchanged
// with subset none
// 4: packs materialize stored payloads (block spacing by ml + ul, entry
// Flags == 0 / CompressedSize == ul instead of the on-wire ml + cl) - the
// layout change invalidates every persisted plan and state file, both are
// rebuilt/reset on the next boot
static const uint32_t VFS_SERIALIZATION_VERSION = 4;

bool vfs_plan_serialize(const VfsPlan &plan, std::vector<uint8_t> &out)
{
    vfs_writer w;
    w.pod(VFS_PLAN_MAGIC);
    w.pod(VFS_SERIALIZATION_VERSION);
    w.pod(plan.product_serial);
    w.str(plan.manifest_repo);
    w.str(plan.file_repo);
    w.pod(uint64_t(plan.files.size()));
    for (const auto &file : plan.files)
    {
        w.str(file.name);
        w.pod(file.size);
        w.pod(uint8_t(file.is_uop ? 1 : 0));
        w.pod(uint64_t(file.assets.size()));
        for (const auto &asset : file.assets)
        {
            w.str(asset.uop_file);
            w.str(asset.name);
            w.pod(asset.uop_hash);
            w.pod(uint64_t(asset.ul));
            w.pod(uint64_t(asset.cl));
            w.pod(asset.ct);
            w.pod(asset.ml);
            w.pod(asset.mc);
            w.pod(asset.rh);
            w.pod(asset.timestamp);
            w.pod(asset.file_offset);
            w.pod(asset.tier);
            w.str(asset.blob);
        }
    }
    // v3: the subset is the trailing field, so a v2 payload is exactly a v3
    // one minus this byte (the backward-compat shape the reader exploits)
    w.pod(plan.subset);
    out = std::move(w.data);
    return true;
}

bool vfs_plan_deserialize(const uint8_t *data, size_t size, VfsPlan &out_plan)
{
    out_plan = VfsPlan();
    vfs_reader r{ data, size, 0 };
    uint32_t magic = 0;
    uint32_t version = 0;
    if (!r.pod(&magic) || !r.pod(&version))
        return false;
    if (magic != VFS_PLAN_MAGIC || version < 2 || version > VFS_SERIALIZATION_VERSION)
        return false; // v1 predates the repos; anything newer is unknown
    if (!r.pod(&out_plan.product_serial))
        return false;
    if (!r.str(&out_plan.manifest_repo) || !r.str(&out_plan.file_repo))
        return false;
    uint64_t file_count = 0;
    if (!r.pod(&file_count))
        return false;
    for (uint64_t i = 0; i < file_count; ++i)
    {
        VfsPlan::VfsFile file;
        uint8_t is_uop = 0;
        uint64_t asset_count = 0;
        if (!r.str(&file.name) || !r.pod(&file.size) || !r.pod(&is_uop) || !r.pod(&asset_count))
            return false;
        file.is_uop = is_uop != 0;
        for (uint64_t j = 0; j < asset_count; ++j)
        {
            VfsAsset asset;
            uint64_t ul = 0, cl = 0;
            if (!r.str(&asset.uop_file) || !r.str(&asset.name) || !r.pod(&asset.uop_hash) ||
                !r.pod(&ul) || !r.pod(&cl) || !r.pod(&asset.ct) || !r.pod(&asset.ml) ||
                !r.pod(&asset.mc) || !r.pod(&asset.rh) || !r.pod(&asset.timestamp) ||
                !r.pod(&asset.file_offset) || !r.pod(&asset.tier) || !r.str(&asset.blob))
                return false;
            asset.ul = size_t(ul);
            asset.cl = size_t(cl);
            file.assets.emplace_back(asset);
        }
        out_plan.files.emplace_back(std::move(file));
    }
    // v3: the recorded subset; v2 plans predate the field - none
    if (version >= 3)
    {
        uint8_t subset = 0;
        if (!r.pod(&subset))
            return false;
        out_plan.subset = subset;
    }
    return r.pos == r.size;
}

bool vfs_plan_save(const fs_path &path, const VfsPlan &plan)
{
    std::vector<uint8_t> data;
    vfs_plan_serialize(plan, data);
    if (!fs_file_write(fs_path_ascii(path), data))
    {
        LOG_ERROR("could not write plan: %s", fs_path_ascii(path));
        return false;
    }
    LOG_DEBUG("plan saved: %s (%zu bytes)", fs_path_ascii(path), data.size());
    return true;
}

bool vfs_plan_load(const fs_path &path, VfsPlan &out_plan)
{
    std::vector<uint8_t> data;
    if (!fs_file_read(fs_path_ascii(path), data))
        return false;
    if (!vfs_plan_deserialize(data.data(), data.size(), out_plan))
    {
        LOG_ERROR("corrupted plan: %s", fs_path_ascii(path));
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// state.bin
// ---------------------------------------------------------------------------

size_t vfs_state_asset_count(const VfsPlan &plan)
{
    size_t count = 0;
    for (const auto &file : plan.files)
        count += file.assets.size();
    return count;
}

bool vfs_state_save(const fs_path &path, const std::vector<uint8_t> &states)
{
    vfs_writer w;
    w.pod(VFS_STATE_MAGIC);
    w.pod(VFS_SERIALIZATION_VERSION);
    w.pod(uint64_t(states.size()));
    w.data.insert(w.data.end(), states.begin(), states.end());
    if (!fs_file_write(fs_path_ascii(path), w.data))
    {
        LOG_ERROR("could not write states: %s", fs_path_ascii(path));
        return false;
    }
    // the save is a commit point (journal rotation folds on it): it must be
    // durable before anything treats state.bin as the source of truth
    if (!vfs_state_file_sync(path))
    {
        LOG_ERROR("could not flush states: %s", fs_path_ascii(path));
        return false;
    }
    return true;
}

bool vfs_state_file_sync(const fs_path &path)
{
    FILE *fp = fopen(fs_path_ascii(path), "r+b");
    if (!fp)
        return false;
    bool ok = fflush(fp) == 0;
#if defined(XUO_WINDOWS)
    ok = ok && _commit(_fileno(fp)) == 0;
#else
    ok = ok && fsync(fileno(fp)) == 0;
#endif
    fclose(fp);
    return ok;
}

bool vfs_state_load(
    const fs_path &path, size_t asset_count, std::vector<uint8_t> &out_states, bool *out_reset)
{
    if (out_reset != nullptr)
        *out_reset = false;
    out_states.assign(asset_count, vfs_pending);
    std::vector<uint8_t> data;
    if (!fs_file_read(fs_path_ascii(path), data))
        return true; // fresh install: everything pending
    vfs_reader r{ data.data(), data.size(), 0 };
    uint32_t magic = 0;
    uint32_t version = 0;
    uint64_t count = 0;
    if (!r.pod(&magic) || !r.pod(&version) || !r.pod(&count) || magic != VFS_STATE_MAGIC ||
        version != VFS_SERIALIZATION_VERSION || count != asset_count || r.pos + count > r.size)
    {
        // plan drift or corruption: unusable, but the resolved answer is
        // still "all pending" (reconcile and materialize rebuild the rest).
        // The caller owes the journal hygiene now: flush this cleared state
        // and truncate the log before any replay resurrects fetched marks
        LOG_WARN("state file unusable (%s), starting from pending", fs_path_ascii(path));
        if (out_reset != nullptr)
            *out_reset = true;
        return true;
    }
    memcpy(out_states.data(), data.data() + r.pos, count);
    return true;
}

// ---------------------------------------------------------------------------
// journal.log
// ---------------------------------------------------------------------------

static bool vfs_journal_sync(FILE *fp, bool durable)
{
    if (fflush(fp) != 0)
        return false;
    if (!durable)
        return true; // flush is enough for process-crash safety; folds fsync
#if defined(XUO_WINDOWS)
    return _commit(_fileno(fp)) == 0;
#else
    return fsync(fileno(fp)) == 0;
#endif
}

bool vfs_journal_open(vfs_journal &journal, const fs_path &path)
{
    journal.handle = fopen(fs_path_ascii(path), "a");
    journal.path = path;
    return journal.handle != nullptr;
}

bool vfs_journal_close(vfs_journal &journal)
{
    const bool ok = journal.handle ? fclose(journal.handle) == 0 : true;
    journal.handle = nullptr;
    return ok;
}

bool vfs_journal_mark(vfs_journal &journal, const vfs_journal_record &record)
{
    if (!journal.handle)
        return false;
    if (record.op == 'F')
        fprintf(journal.handle, "F %u %u\n", record.file_index, record.asset_index);
    else if (record.op == 'S')
        fprintf(journal.handle, "S %u\n", record.file_index);
    else if (record.op == 'B')
        fprintf(journal.handle, "B %u %u\n", record.file_index, record.asset_index);
    else if (record.op == 'R')
        fprintf(journal.handle, "R %u %u\n", record.file_index, record.asset_index);
    else
        return false;
    // every completion mark is durable before it is reported as done (when
    // the journal runs durable; runtime marks flush only - see vfs_journal)
    return vfs_journal_sync(journal.handle, journal.durable);
}

bool vfs_journal_replay(const fs_path &path, std::vector<vfs_journal_record> &out)
{
    out.clear();
    FILE *fp = fopen(fs_path_ascii(path), "r");
    if (!fp)
        return true; // no journal yet: nothing to replay
    char line[128] = {};
    while (fgets(line, sizeof(line), fp))
    {
        vfs_journal_record record;
        if (line[0] == 'F')
        {
            unsigned int file_index = 0, asset_index = 0;
            if (sscanf(line, "F %u %u", &file_index, &asset_index) == 2)
            {
                record = { 'F', file_index, asset_index };
                out.push_back(record);
            }
        }
        else if (line[0] == 'S')
        {
            unsigned int file_index = 0;
            if (sscanf(line, "S %u", &file_index) == 1)
            {
                record = { 'S', file_index, 0 };
                out.push_back(record);
            }
        }
        else if (line[0] == 'B')
        {
            unsigned int file_index = 0, asset_index = 0;
            if (sscanf(line, "B %u %u", &file_index, &asset_index) == 2)
            {
                record = { 'B', file_index, asset_index };
                out.push_back(record);
            }
        }
        else if (line[0] == 'R')
        {
            unsigned int file_index = 0, asset_index = 0;
            if (sscanf(line, "R %u %u", &file_index, &asset_index) == 2)
            {
                record = { 'R', file_index, asset_index };
                out.push_back(record);
            }
        }
        // anything else (a torn trailing write) is ignored
    }
    fclose(fp);
    return true;
}

// ---------------------------------------------------------------------------
// reconcile
// ---------------------------------------------------------------------------

// applies fetched journal records on top of the given states; out of range
// records (stale or corrupted journal) are ignored
void vfs_state_apply_journal(
    const VfsPlan &plan,
    const std::vector<vfs_journal_record> &records,
    std::vector<uint8_t> &states)
{
    for (const auto &record : records)
    {
        if (record.file_index >= plan.files.size())
            continue;
        const auto &file = plan.files[record.file_index];
        if (record.asset_index >= file.assets.size())
            continue;
        size_t asset_index = record.asset_index;
        for (size_t fi = 0; fi < record.file_index; ++fi)
            asset_index += plan.files[fi].assets.size();
        if (asset_index >= states.size())
            continue;
        // 'F': block written; 'B': placeholder served; 'R': reset to
        // pending by the update/diff flow. A stub mark never overrides
        // fetched state (a heal that raced the report wins)
        if (record.op == 'F')
            states[asset_index] = vfs_fetched;
        else if (record.op == 'B' && states[asset_index] != vfs_fetched)
            states[asset_index] = vfs_stubbed;
        else if (record.op == 'R')
            states[asset_index] = vfs_pending;
    }
}

static bool vfs_state_file_size(const fs_path &path, size_t *out_size)
{
    FILE *fp = fopen(fs_path_ascii(path), "rb");
    if (!fp)
        return false;
    bool ok = false;
#if defined(XUO_WINDOWS)
    if (_fseeki64(fp, 0, SEEK_END) == 0)
    {
        const __int64 size = _ftelli64(fp);
        ok = size >= 0 && size_t(size) == uint64_t(size);
        *out_size = size_t(size);
    }
#else
    if (fseeko(fp, 0, SEEK_END) == 0)
    {
        const off_t size = ftello(fp);
        ok = size >= 0;
        *out_size = size_t(size);
    }
#endif
    fclose(fp);
    return ok;
}

bool vfs_state_reconcile(
    const VfsPlan &plan,
    const fs_path &install_dir,
    std::vector<uint8_t> &states,
    vfs_reconcile_stats *stats,
    const fs_path &blobs_dir)
{
    vfs_reconcile_stats run;
    size_t asset_index = 0;
    for (size_t fi = 0; fi < plan.files.size(); ++fi)
    {
        const auto &file = plan.files[fi];
        const auto path = fs_path_join(install_dir, file.name);

        const uint8_t *mapped = nullptr;
        size_t mapped_size = 0;
        if (file.is_uop && fs_path_exists(path))
        {
            mapped = fs_map(path, &mapped_size);
        }

        for (size_t ai = 0; ai < file.assets.size(); ++ai, ++asset_index)
        {
            const auto &asset = file.assets[ai];
            if (asset_index >= states.size())
                break;
            ++run.assets_checked;

            // the on-disk file is the source of truth: downgrade fetched
            // state the disk does not confirm, upgrade pending state it does
            // (a crash between the block write and the journal mark)
            bool on_disk = false;
            if (file.is_uop)
            {
                on_disk = vfs_store_pack_asset_present(mapped, uint64_t(mapped_size), ai, asset);
            }
            else
            {
                size_t loose_size = 0;
                on_disk = vfs_asset_block_size(asset) > 0 &&
                          vfs_state_file_size(path, &loose_size) && loose_size == asset.ul;
                // the size check alone also accepts the untouched boot
                // skeleton (right-sized sparse file). When the store root is
                // known, the blob content decides - the skeleton must not
                // promote, and a blob that arrived later must not be shadowed
                // by it forever
                if (on_disk && !fs_path_empty(blobs_dir))
                {
                    on_disk = vfs_store_loose_asset_content_ok(path, asset, blobs_dir);
                }
            }

            if (states[asset_index] == vfs_fetched && !on_disk)
            {
                LOG_TRACE("reconcile: downgrade %s[%zu] to pending", file.name.c_str(), ai);
                states[asset_index] = vfs_pending;
                ++run.assets_repaired;
            }
            else if (states[asset_index] == vfs_pending && on_disk)
            {
                // promotion needs the real block: a stub patch passes the
                // entry-table check for ct==0 shapes, so confirm content
                // against the blob when the store root is known
                const bool real =
                    !file.is_uop || vfs_store_pack_asset_present(
                                        mapped, uint64_t(mapped_size), ai, asset, blobs_dir);
                if (real)
                {
                    LOG_TRACE("reconcile: upgrade %s[%zu] to fetched", file.name.c_str(), ai);
                    states[asset_index] = vfs_fetched;
                    ++run.assets_upgraded;
                }
            }
            // vfs_stubbed: stays stubbed. On-disk presence alone never promotes
            // a placeholder (the entry patch is entry-identical to the plan for
            // ct==0 assets - m4 review); only the heal path (the materialize
            // write + its journal 'F') moves stubbed to fetched
        }
        if (mapped)
        {
            fs_unmap(const_cast<unsigned char *>(mapped), mapped_size);
        }
    }

    if (stats)
        *stats = run;
    return true;
}

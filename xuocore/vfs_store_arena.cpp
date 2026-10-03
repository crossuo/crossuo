// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_store_arena: the granule-resident memory backend (web groundwork).
// See the header for the residency, eviction and consumer-liveness rules.

#define LOGGER_MODULE Vfs

#include "vfs_store_arena.h"

#include <inttypes.h>

#include <algorithm>
#include <string.h>

#include "common.h"
#include <common/logging/logging.h>
#include <external/zlib_amalg.h>
#include "vfs_fetch.h"
#include "vfs_manifest.h"
#include "vfs_stub.h"
#include "vfs_transport.h"

// demand-tier fetch priority (lower = more urgent: T0 uses 0, T1 uses 1)
#define VFS_ARENA_DEMAND_PRIO 2

vfs_store_arena::vfs_store_arena(const vfs_store_arena_params &params)
    : m_params(params)
{
}

vfs_store_arena::~vfs_store_arena() = default;

bool vfs_store_arena::open(const VfsPlan &plan)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_plan = &plan;
    LOG_INFO(
        "arena: open (%zu files, file repo: '%s')",
        plan.files.size(),
        plan.file_repo.c_str());
    m_granules.clear();
    m_pack_index.clear();
    m_loose_index.clear();
    m_sealed.assign(plan.files.size(), false);
    m_bytes_resident = 0;
    m_clock = 0;
    for (size_t fi = 0; fi < plan.files.size(); ++fi)
    {
        const auto &file = plan.files[fi];
        if (file.streamable)
        {
            // subset-retained metadata: no bytes anywhere for the arena -
            // the on-demand stream lane acquires these
            continue;
        }
        for (size_t ai = 0; ai < file.assets.size(); ++ai)
        {
            const auto &asset = file.assets[ai];
            vfs_arena_granule granule;
            granule.file_index = uint32_t(fi);
            granule.asset_index = uint32_t(ai);
            granule.hole = vfs_asset_is_hole(asset);
            const size_t index = m_granules.size();
            if (vfs_asset_is_loose(asset))
                m_loose_index[asset.name] = index;
            else
                m_pack_index[asset.uop_hash] = index;
            m_granules.push_back(granule);
        }
    }
    return true;
}

bool vfs_store_arena::locate(const VfsAsset &asset, size_t *granule_index) const
{
    // assumes m_mutex held
    if (!m_plan)
        return false;
    if (vfs_asset_is_loose(asset))
    {
        const auto found = m_loose_index.find(asset.name);
        if (found == m_loose_index.end())
            return false;
        *granule_index = found->second;
        return true;
    }
    const auto found = m_pack_index.find(asset.uop_hash);
    if (found == m_pack_index.end())
        return false;
    *granule_index = found->second;
    return true;
}

void vfs_store_arena::touch_locked(vfs_arena_granule &granule)
{
    granule.last_access = ++m_clock;
}

void vfs_store_arena::evict_locked(uint64_t bytes_target)
{
    // assumes m_mutex held; LRU over last_access, in-flight ensures skipped
    while (m_bytes_resident > bytes_target)
    {
        vfs_arena_granule *victim = nullptr;
        for (auto &granule : m_granules)
        {
            if (!granule.resident || granule.inflight)
                continue;
            if (victim == nullptr || granule.last_access < victim->last_access)
                victim = &granule;
        }
        if (victim == nullptr)
            break; // everything resident is in flight: nothing to drop
        m_bytes_resident -= victim->bytes.size();
        std::vector<uint8_t>().swap(victim->bytes);
        victim->resident = false;
        victim->last_access = 0;
        ++m_evictions;
    }
}

bool vfs_store_arena::ensure(const VfsAsset &asset, bool block)
{
    size_t index = 0;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!locate(asset, &index))
            return false;
        vfs_arena_granule &granule = m_granules[index];
        if (granule.hole)
            return true; // reserved slot: trivially ready (sparse parity)
        touch_locked(granule);
        if (granule.resident)
            return true;
        // a resident STUB granule falls through: ensure() is the heal path
        // (zero-latency), so a real blob re-materializes over the stub bytes
        if (granule.inflight)
        {
            // another thread is loading exactly this granule
            if (!block)
                return false;
            m_cv.wait(lock, [&granule] { return !granule.inflight; });
            touch_locked(granule);
            return granule.resident;
        }
        granule.inflight = true; // held across the load: eviction stays out
    }

    // outside the lock: the load itself (file io + inflate). A blocking
    // ensure runs the blob fetch first when the engine can (the trap's
    // "fetch, then materialize" order, pulled inside the backend); a
    // non-blocking one queues the demand fetch (the heal source) and
    // reports the miss immediately.
    const uint64_t expected = asset.ct == 0 ? uint64_t(asset.ul) : uint64_t(asset.cl);
    const bool in_store = fs_path_exists(fs_path_join(m_params.store.blobs_dir, asset.blob));
    std::vector<uint8_t> payload;
    bool loaded = false;
    if (asset.blob_offset != 0 && !in_store)
    {
        // refined block: stream [ml][payload] straight from the range source.
        // The store never keeps these sources (up to 476MB); pulling one
        // whole for a 10KB block defeats streaming and blows the fetch caps
        // (the cap travels with the block size). Non-blocking misses report
        // the miss: the next blocking ensure streams the block.
        if (block && m_plan != nullptr && vfs_fetch_started())
        {
            const uint64_t len = vfs_asset_range_len(asset);
            const astr_t url = m_plan->file_repo + asset.blob;
            std::vector<uint8_t> block_bytes(size_t(len), 0);
            const vfs_transport_result res = vfs_transport_active()->range(
                url.c_str(),
                asset.blob.c_str(),
                asset.blob_offset,
                len,
                [](void *context, const uint8_t *data, size_t size)
                {
                    static_cast<std::vector<uint8_t> *>(context)->assign(data, data + size);
                    return true;
                },
                &block_bytes,
                0,
                false);
            // the block is [ml metadata][payload] (pack_write_block's range
            // layout): the granule holds the payload only
            if (res.ok && res.bytes == len && block_bytes.size() >= asset.ml)
            {
                const uint8_t *payload_src = block_bytes.data() + asset.ml;
                const size_t payload_len = block_bytes.size() - asset.ml;
                if (asset.ct == 1)
                {
                    payload.resize(asset.ul);
                    uLongf dst_len = payload.size();
                    loaded = uncompress(
                                 payload.data(),
                                 &dst_len,
                                 reinterpret_cast<Bytef const *>(payload_src),
                                 uLong(payload_len)) == Z_OK;
                    if (loaded)
                        payload.resize(size_t(dst_len));
                }
                else
                {
                    payload.assign(payload_src, payload_src + payload_len);
                    loaded = true;
                }
            }
            if (!loaded)
                LOG_ERROR(
                    "arena: ranged ensure failed (%s+%s, len %llu, ok %d, bytes %llu)",
                    m_plan->file_repo.c_str(),
                    asset.blob.c_str(),
                    (unsigned long long)len,
                    int(res.ok),
                    (unsigned long long)res.bytes);
        }
    }
    else
    {
        if (block && !in_store)
        {
            if (vfs_fetch_started())
                vfs_fetch_blob_to_store(asset.blob.c_str(), expected, false, m_params.fetch_timeout_ms);
        }
        else if (!block && !in_store && vfs_fetch_started())
        {
            vfs_fetch_queue_blob(asset.blob.c_str(), expected, false, VFS_ARENA_DEMAND_PRIO);
        }
        loaded = vfs_store_asset_payload(m_params.store.blobs_dir, asset, &payload) == 0;
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    vfs_arena_granule &granule = m_granules[index];
    granule.inflight = false;
    const bool ok = loaded;
    if (ok)
    {
        granule.bytes = std::move(payload);
        granule.resident = true;
        m_bytes_resident += granule.bytes.size();
        touch_locked(granule);
        evict_locked(m_params.max_bytes); // the budget holds after every commit
    }
    m_cv.notify_all();
    return ok;
}

void vfs_store_arena::prefetch(const VfsAsset &asset, int prio)
{
    // one request per granule, no coalescing (HTTP/2 multiplexes many small
    // GETs fine); the engine dedups by
    // tail. The granule heals when the blob lands (ensure re-reads it).
    if (vfs_asset_is_hole(asset) || asset.blob_offset != 0)
        return; // ranged blocks stream on ensure: their source is never a blob
    const uint64_t expected = asset.ct == 0 ? uint64_t(asset.ul) : uint64_t(asset.cl);
    vfs_fetch_queue_blob(asset.blob.c_str(), expected, false, prio);
}

void vfs_store_arena::evict(size_t bytes_target)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const uint64_t before = m_bytes_resident;
    evict_locked(bytes_target);
    if (m_bytes_resident < before)
        LOG_TRACE("arena: evicted %" PRIu64 " bytes", before - m_bytes_resident);
}

bool vfs_store_arena::is_ready(const VfsAsset &asset) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t index = 0;
    if (!locate(asset, &index))
        return false;
    const vfs_arena_granule &granule = m_granules[index];
    return granule.hole || granule.resident;
}

void vfs_store_arena::seal_check()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<bool> sealed(m_plan->files.size(), true);
    for (const auto &granule : m_granules)
    {
        if (!granule.hole && !granule.resident)
            sealed[granule.file_index] = false;
    }
    m_sealed = std::move(sealed);
}

bool vfs_store_arena::read(const VfsAsset &asset, size_t offset, uint8_t *dst, size_t length)
{
    if (dst == nullptr && length != 0)
        return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t index = 0;
    if (!locate(asset, &index))
        return false;
    vfs_arena_granule &granule = m_granules[index];
    if (granule.hole || !granule.resident)
        return false;
    if (offset > granule.bytes.size() || length > granule.bytes.size() - offset)
        return false;
    touch_locked(granule);
    if (length != 0)
        memcpy(dst, granule.bytes.data() + offset, length);
    return true;
}

void vfs_store_arena::invalidate(const VfsAsset &asset)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t index = 0;
    if (!locate(asset, &index))
        return;
    vfs_arena_granule &granule = m_granules[index];
    if (granule.resident)
    {
        m_bytes_resident -= granule.bytes.size();
        std::vector<uint8_t>().swap(granule.bytes);
        granule.resident = false;
        granule.last_access = 0;
    }
    m_sealed[granule.file_index] = false;
}

bool vfs_store_arena::file_sealed(size_t file_index) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return file_index < m_sealed.size() && m_sealed[file_index];
}

vfs_arena_stats vfs_store_arena::stats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    vfs_arena_stats out;
    for (const auto &granule : m_granules)
    {
        if (granule.hole)
            continue;
        ++out.granules;
        if (granule.resident)
        {
            ++out.resident;
        }
    }
    out.bytes_resident = m_bytes_resident;
    out.evictions = m_evictions;
    return out;
}

vfs_store_arena *vfs_store_backend_arena_create(const vfs_store_arena_params &params)
{
    // the backend binds to a plan through open() before first use
    return new vfs_store_arena(params);
}

// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

#define LOGGER_MODULE Vfs

// include-order workaround: mft.h pulls tinyxml2.h, which must come before
// any header that pulls common/logging.h (vfs.h does)
#include "mft.h"
#include "vfs_source_aops.h"

#include "vfs.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h> // sbrk (web heap probe)

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(XUO_WINDOWS)
#include <windows.h> // MoveFileExA (atomic product manifest replace)
#endif
#if defined(XUO_EMSC)
#include <emscripten.h>
#include <emscripten/threading.h>
#include <pthread.h>
#include <external/zlib_amalg.h>
#include <external/lookup3.h> // anim.mul CDN blob name (loose-file hash addressing)
using zlib_uLongf = uLongf;
using zlib_Bytef = Bytef;
using zlib_uLong = uLong;
#else
using zlib_uLongf = size_t;
using zlib_Bytef = uint8_t;
using zlib_uLong = size_t;
#endif
#include "uop.h"

#include "http.h"
#include "vfs_fetch.h"
#include "vfs_record.h"
#include "vfs_store_arena.h"
#include "vfs_trap.h"
#include "vfs_transport.h"

#include "common.h"
#include "uodata.h"
#include <common/logging/logging.h>

#define VFS_PRODUCT_FILE_DEFAULT "uo-legacyrelease.prod"
// the official EA product manifest, used when no explicit URL
#define VFS_PRODUCT_URL_DEFAULT                                                                    \
    "http://patch.uo.eamythic.com/uopatch-sa/legacyrelease/uo/manifest/uo-legacyrelease.prod"
// background tier fetch+materialize chunking
#define VFS_BOOT_T1_CHUNK 32
// cap of expected bytes admitted per T1 chunk: bounds how long the state
// lock is held per materialize (the game's mmaps and traps queue behind it)
#define VFS_BOOT_T1_CHUNK_BYTES (16 * 1024 * 1024)
// post-frame convergence: byte budget per batch, pause between batches and
// the state fold interval (durability points during long convergences)
#define VFS_CONV_BATCH_BYTES (8 * 1024 * 1024)
#define VFS_CONV_BATCH_PAUSE_MS 15
#define VFS_CONV_FOLD_BYTES (64 * 1024 * 1024)
#define VFS_CONV_FOLD_SECS 30.0
// poll interval while the convergence waits for the first frame
#define VFS_CONV_WAIT_POLL_MS 50

static vfs_params s_params;
static VfsPlan s_plan;
static bool s_plan_loaded = false;
#if defined(XUO_EMSC)
// web boot completion: the splash polls this while the streaming install runs
// (declared early: the convergence loop reports through it). The progress
// VALUE lives in the transport byte accounting (http.cpp) - vfs_boot_web_progress
// derives the ratio from it, so no float is published from the boot paths
// anymore (every old writer fought the live byte ratio)
static std::atomic<bool> s_boot_web_done{ false };
// assets whose blob never made it into a materialized pack (zero slots read
// as zeros in game): the boot log and the splash report it
static size_t s_boot_web_stub_assets = 0;
#endif

// boot state (vfs_boot/vfs_shutdown own these; the background tier and the
// fetch workers only touch vfs_fetch/store/state internals under s_state_mutex)
static std::atomic<bool> s_boot_started{ false };
static std::atomic<bool> s_boot_shutdown{ false };
// set once the game rendered its first frame: the background worker may then
// spend disk/bandwidth on post-frame convergence (it never does before: boot
// work must not compete with the first frame)
static std::atomic<bool> s_frame_seen{ false };
static std::thread s_background;
static std::mutex s_state_mutex;
static std::vector<uint8_t> s_states;
static vfs_journal s_journal;
// granule store: the params-selected backend. The native
// default (sparse) never instantiates it - the disk pipeline is the standard
// one; arena mode keeps its granules here for the boot's ensure paths
static vfs_store_arena *s_arena = nullptr;

// managed-mapping registry: plan file name (lowercased) -> file index.
// Built when the plan is loaded; consumed by vfs_map_file.
static std::map<astr_t, size_t> s_managed_files;
static bool s_managed_built = false;

// pack-asset registry: uop hash -> (file index, asset index) for the
// index-build stubbed lookups (vfs_asset_is_stubbed)
static std::map<uint64_t, std::pair<size_t, size_t>> s_managed_hashes;


// ---------------------------------------------------------------------------
// managed mappings
// ---------------------------------------------------------------------------

static astr_t vfs_lower_ascii(const char *text)
{
    astr_t lower(text);
    std::transform(
        lower.begin(),
        lower.end(),
        lower.begin(),
        [](unsigned char c) { return char(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c); });
    for (auto &c : lower)
    {
        if (c == '\\')
            c = '/';
    }
    return lower;
}

// the plan module state: loads (or rebuilds) s_plan on first use
static bool vfs_plan_ready()
{
    if (s_plan_loaded)
        return true;
    VfsPlan plan;
    return vfs_plan_ensure(plan); // ensure() binds s_plan on success
}

static void vfs_managed_build()
{
    if (s_managed_built || !vfs_plan_ready())
        return;
    s_managed_built = true;
    s_managed_files.clear();
    s_managed_hashes.clear();
    for (size_t i = 0; i < s_plan.files.size(); ++i)
    {
        s_managed_files[vfs_lower_ascii(s_plan.files[i].name.c_str())] = i;
        const auto &file = s_plan.files[i];
        for (size_t ai = 0; ai < file.assets.size(); ++ai)
        {
            if (file.assets[ai].uop_hash != 0)
                s_managed_hashes[file.assets[ai].uop_hash] = { i, ai };
        }
    }
}

// global plan asset index of a file's first asset (state bytes are in plan
// order: files in plan order, assets in file order)
static size_t vfs_state_base_index(const VfsPlan &plan, size_t file_index)
{
    size_t index = 0;
    for (size_t i = 0; i < file_index && i < plan.files.size(); ++i)
        index += plan.files[i].assets.size();
    return index;
}

// plan file index for a mapped path, -1 when the path is not a managed
// artifact: it must live inside the install dir and match a plan file name
static int64_t vfs_managed_index_of(const fs_path &path)
{
    vfs_managed_build(); // binds the registry to the current params' plan
    if (!vfs_plan_ready() || s_managed_files.empty())
        return -1;
    astr_t candidate = vfs_lower_ascii(fs_path_ascii(path));
    astr_t install = vfs_lower_ascii(fs_path_ascii(s_params.install_dir));
    if (!install.empty() && install.back() != '/')
        install += '/';
    if (candidate.size() <= install.size() || candidate.compare(0, install.size(), install) != 0)
        return -1;
    const astr_t name = candidate.substr(install.size());
    const auto found = s_managed_files.find(name);
    if (found == s_managed_files.end())
        return -1;
    return int64_t(found->second);
}

// trap resolution backend: acquire the blob (store-only when offline; the
// fetch engine dedups equal in-flight tails), then run the shared write path
// for exactly this asset. Called with no trap lock held (see the vfs_trap.h
// lock order).
// The wait is bounded: a fetch stalled past this turns the fault into
// the stub path instead of parking the faulting thread forever
#define VFS_TRAP_FETCH_TIMEOUT_MS (30 * 1000)

// fetches one asset's bytes and materializes it through the shared write
// path: range assets stream their own block ([ml metadata][payload], HTTP
// Range or a ranged read of a cached blob), everything else fetches the
// whole blob into the store first. The trap's fault resolution and the T1
// worker's prefetch both land here. fault_offset is diagnostic only.
static bool vfs_fetch_and_materialize_asset(
    uint32_t file_index,
    uint32_t asset_index,
    uint64_t fault_offset,
    bool record = true,
    int fetch_priority = VFS_FETCH_PRIO_DEMAND)
{
    if (!vfs_plan_ready() || file_index >= s_plan.files.size())
        return false;
    const auto &file = s_plan.files[file_index];
    if (asset_index >= file.assets.size())
        return false;
    const auto &asset = file.assets[asset_index];
    const uint64_t expected = asset.ct == 0 ? uint64_t(asset.ul) : uint64_t(asset.cl);
    const bool ranged = asset.blob_offset != 0;
    const auto ensure_start = std::chrono::steady_clock::now();

    vfs_store_params store_params;
    store_params.blobs_dir = s_params.blobs_dir;
    store_params.install_dir = s_params.install_dir;
    store_params.state_dir = s_params.state_dir;
    store_params.record = record; // T1 prefetch is cache-warming, not usage
    store_params.quiet = true;    // the resolve outcome is logged below; the
                                  // per-run stats line would be pure spam

    const auto log_resolve = [&](bool ok)
    {
        const double ensure_secs =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - ensure_start).count();
        if (ok && expected < 1024 * 1024)
        {
            // successful small resolves stay quiet (DEBUG); failures are
            // always visible - a silently missing asset is a bug report
            DEBUG(
                Vfs,
                "trap resolve: %s (%u: %llu bytes, fault@%llu) in %.3fs",
                vfs_asset_is_loose(asset) ? asset.name.c_str() : file.name.c_str(),
                asset_index,
                (unsigned long long)expected,
                (unsigned long long)fault_offset,
                ensure_secs);
            return;
        }
        // large on-demand resolves land on the critical path of whatever read
        // faulted: keep them visible at INFO
        LOG_INFO(
            "trap resolve: %s (%u: %llu bytes, fault@%llu) in %.3fs%s",
            vfs_asset_is_loose(asset) ? asset.name.c_str() : file.name.c_str(),
            asset_index,
            (unsigned long long)expected,
            (unsigned long long)fault_offset,
            ensure_secs,
            ok ? "" : " FAILED");
    };

    // range assets stream their block ([ml metadata][payload]) straight from
    // the source file when the whole blob is not cached locally: no 264MB
    // downloads for a 10KB block
    if (ranged && !fs_path_exists(fs_path_join(s_params.blobs_dir, asset.blob)))
    {
        if (s_params.offline)
            return false; // nothing to stream from; degrade to the stub path
        const uint64_t len = vfs_asset_range_len(asset);
        astr_t url = astr_t(s_plan.file_repo) + asset.blob;
        std::vector<uint8_t> bytes(static_cast<size_t>(len), 0);
        // the sink is what lands the transfer body: without it the transport
        // reports success with zero bytes and the block would stub out
        vfs_transport_result res = vfs_transport_active()->range(
            url.c_str(), asset.blob.c_str(), asset.blob_offset, len,
            [](void *context, const uint8_t *data, size_t size)
            {
                static_cast<std::vector<uint8_t> *>(context)->assign(data, data + size);
                return true;
            },
            &bytes, 0, false);
        const bool ok = res.ok && res.bytes == len;
        log_resolve(ok);
        if (!ok)
            return false;
        std::unique_lock<std::mutex> lock(s_state_mutex);
        return vfs_store_materialize_asset_data(
            s_plan,
            store_params,
            s_states,
            &s_journal,
            file_index,
            asset_index,
            bytes);
    }

    bool fetched = true;
    if (!s_params.offline && !fs_path_exists(fs_path_join(s_params.blobs_dir, asset.blob)))
    {
        if (!vfs_fetch_started())
            return false; // no engine: nothing can appear in the store
        const uint64_t wire = asset.ct == 0 ? uint64_t(asset.ul) : uint64_t(asset.cl);
        fetched =
            vfs_fetch_blob_to_store(asset.blob.c_str(), wire, false, VFS_TRAP_FETCH_TIMEOUT_MS, fetch_priority);
    }
    log_resolve(fetched);
    if (!fetched)
        return false; // the fault degrades to the stub path
    std::unique_lock<std::mutex> lock(s_state_mutex);
    return vfs_store_rematerialize_asset(
        s_plan, store_params, s_states, &s_journal, file_index, asset_index);
}

static bool vfs_trap_ensure_asset(void *context, const vfs_trap_block *block)
{
    (void)context;
    if (!block || block->file_index >= s_plan.files.size())
        return false;
    return vfs_fetch_and_materialize_asset(
        block->file_index, block->asset_index, block->offset);
}

// stub transition: the trap served a placeholder payload for this asset.
// Folds the state byte + a durable journal mark so the next boot sees the
// placeholder truth (and the index build can mark the game's caches)
static void vfs_trap_asset_stubbed(void *context, const vfs_trap_block *block)
{
    (void)context;
    if (!block || !vfs_plan_ready() || block->file_index >= s_plan.files.size())
        return;
    const auto &file = s_plan.files[block->file_index];
    if (block->asset_index >= file.assets.size())
        return;
    size_t global = block->asset_index;
    for (size_t fi = 0; fi < block->file_index; ++fi)
        global += s_plan.files[fi].assets.size();
    std::unique_lock<std::mutex> lock(s_state_mutex);
    if (global >= s_states.size() || s_states[global] == vfs_fetched)
        return; // a heal that raced the report wins
    s_states[global] = vfs_stubbed;
    vfs_journal_mark(s_journal, { 'B', block->file_index, block->asset_index });
}

static void vfs_trap_backend_ensure()
{
    vfs_trap_backend backend;
    backend.context = nullptr;
    backend.ensure = vfs_trap_ensure_asset;
    backend.stubbed = vfs_trap_asset_stubbed;
    vfs_trap_set_backend(backend);
}

bool vfs_map_file(const fs_path &path, uint8_t *&out_start, size_t &out_size)
{
    out_start = nullptr;
    out_size = 0;
    if (vfs_enabled())
    {
        const int64_t index = vfs_managed_index_of(path);
        // streamable files (subset-retained metadata, e.g. the legacy anim
        // muls on web) never map through the managed/trap path: their bytes
        // belong to the on-demand stream lane, and the file is absent from
        // the install until that lane completes it
        if (index >= 0 && !s_plan.files[size_t(index)].streamable)
        {
            vfs_trap_backend_ensure();
            // state snapshot for the file (loose presence comes from the
            // state DB); taken and released before mapping so the store lock
            // is never held across the trap's own lock (trap->store order)
            std::vector<uint8_t> states;
            const size_t assets = s_plan.files[size_t(index)].assets.size();
            {
                std::unique_lock<std::mutex> lock(s_state_mutex);
                const size_t base = vfs_state_base_index(s_plan, size_t(index));
                if (base < s_states.size())
                {
                    const size_t count = std::min(assets, s_states.size() - base);
                    states.assign(
                        s_states.begin() + ptrdiff_t(base),
                        s_states.begin() + ptrdiff_t(base + count));
                }
            }
            out_start = vfs_trap_map(
                fs_path_join(s_params.install_dir, s_plan.files[size_t(index)].name),
                s_plan,
                size_t(index),
                states.size() == assets ? states.data() : nullptr,
                &out_size);
            if (out_start)
                return true;
            LOG_WARN("vfs: managed mapping failed, falling back: %s", fs_path_ascii(path));
        }
    }
    // unmanaged path (or streaming disabled): exactly the pre-VFS behavior
    out_start = reinterpret_cast<uint8_t *>(fs_map(path, &out_size));
    return out_start != nullptr;
}

void vfs_unmap_file(uint8_t *start, size_t size)
{
    if (vfs_enabled() && vfs_trap_unmap(start))
        return;
    fs_unmap(reinterpret_cast<unsigned char *>(start), size);
}

bool vfs_ensure_range(uint8_t *base, size_t offset, size_t length)
{
    if (!vfs_enabled() || base == nullptr || length == 0)
        return true; // disabled: no-op, zero behavior change
    return vfs_trap_resolve_range(base, uint64_t(offset), uint64_t(length));
}

// ---------------------------------------------------------------------------
// stubs and hot-swap
// ---------------------------------------------------------------------------

uint64_t vfs_stub_count()
{
    vfs_trap_stats stats;
    vfs_trap_stats_get(&stats);
    return stats.stubbed;
}

bool vfs_is_ready(uint8_t *base, size_t offset)
{
    if (!vfs_enabled() || base == nullptr)
        return true; // native files are always whole
    const auto mark = vfs_trap_page_mark_at(base, uint64_t(offset));
    return mark == vfs_trap_mark_real;
}

bool vfs_refresh_block(const uint8_t *addr)
{
    if (!vfs_enabled() || addr == nullptr)
        return false;
    return vfs_trap_asset_healed(addr);
}

bool vfs_refresh_index(CIndexObject *io, void (*destroy)(CIndexObject *obj))
{
    if (io == nullptr || !vfs_enabled() || io->Stubbed == 0)
        return false;
    // Address is the payload pointer the index was built from: it identifies
    // the trap asset whose real bytes may have landed in the meantime
    if (io->Address == 0 || !vfs_refresh_block(reinterpret_cast<const uint8_t *>(io->Address)))
        return false;
    if (destroy != nullptr)
        destroy(io); // game-supplied teardown (DestroySprite and friends)
    io->UserData = nullptr;
    // UOP gumps are the one kind whose index carries payload-derived fields:
    // UopReadIndexFile read Width/Height from the block's first 8 bytes (the
    // stub wrote 0x0 there so the readers short-circuit) and advanced Address
    // past them - so the dims sit 8 bytes behind Address. Re-read them from
    // the healed bytes before the caller reloads, mirroring that bake exactly.
    if (io->UopBlock != nullptr &&
        vfs_trap_stub_kind_at(reinterpret_cast<const uint8_t *>(io->Address)) == vfs_stub_gump)
    {
        const uint8_t *dims = reinterpret_cast<const uint8_t *>(io->Address) - 8;
        io->Width = int32_t(dims[0]) | int32_t(dims[1]) << 8 | int32_t(dims[2]) << 16 |
                    int32_t(dims[3]) << 24;
        io->Height = int32_t(dims[4]) | int32_t(dims[5]) << 8 | int32_t(dims[6]) << 16 |
                     int32_t(dims[7]) << 24;
    }
    io->Stubbed = 0;
    return true; // caller must reload
}

// gump dims resolution for streamed hole-baked entries: the index bakes the
// block's planned address with unknown dimensions (they ride in the block's
// first words), and the readers short-circuit on zero dims BEFORE touching
// the data - so nothing ever triggered the fetch. Dereferencing through the
// trap here fetches the block on first use; the dims then cache on the
// entry exactly like the materialized path bakes them. Returns true when
// the dims are (now) valid.
bool vfs_gump_dims_resolve(CIndexObject *io)
{
    if (io == nullptr || !vfs_enabled() || !vfs_plan_ready())
        return false;
    if (io->Width != 0 && io->Height != 0)
        return true; // materialized at index build or already resolved
#if defined(XUO_EMSC)
    // web: there are no fault-driven probes - the pack is a table-only
    // skeleton and a raw read of io->Address returns the mapping's zero
    // page. Resolve stored-block dims through the punctual fetch instead;
    // the payload lands in the LRU, so the pixel fetch that follows is a
    // cache hit. Compressed blocks keep decoding at the read path.
    if (io->UopBlock != nullptr && io->UopBlock->Flags == 0)
    {
        const auto payload = vfs_asset_fetch_block_payload(io->UopBlock->Hash, io->UopBlock, false);
        if (payload == nullptr || payload->size() < 8)
            return false;
        uint32_t width = 0, height = 0;
        memcpy(&width, payload->data(), sizeof(width));
        memcpy(&height, payload->data() + 4, sizeof(height));
        constexpr uint32_t maxGumpDimension = 2048;
        if (width == 0 || height == 0 || width > maxGumpDimension || height > maxGumpDimension)
            return false;
        io->Width = int32_t(width);
        io->Height = int32_t(height);
        return true; // Address stays 0: the pixels come from the fetch layer
    }
#endif
    if (io->Address == 0u)
        return false; // nodraw hole: genuinely no asset
    volatile const uint8_t *probe = reinterpret_cast<const volatile uint8_t *>(io->Address);
    (void)probe[0]; // the fault fetches the covering block synchronously
    (void)probe[4];
    const uint32_t width = *reinterpret_cast<const uint32_t *>(io->Address);
    const uint32_t height = *reinterpret_cast<const uint32_t *>(io->Address + 4);
    constexpr uint32_t maxGumpDimension = 2048;
    if (width == 0 || height == 0 || width > maxGumpDimension || height > maxGumpDimension)
        return false; // fetch failed: the stub zeros stay, retried on next use
    io->Width = int32_t(width);
    io->Height = int32_t(height);
    io->Address += 8; // the readers start past the dims, as materialized entries do
    io->DataSize = io->DataSize > 8 ? io->DataSize - 8 : 0;
    return true;
}

// state byte of one plan asset (a snapshot read; the byte is only advisory
// for the game's index markers)
static bool vfs_asset_state_is(uint32_t file_index, uint32_t asset_index, uint8_t state)
{
    if (!vfs_plan_ready() || file_index >= s_plan.files.size())
        return false;
    size_t global = asset_index;
    for (size_t fi = 0; fi < file_index; ++fi)
        global += s_plan.files[fi].assets.size();
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return global < s_states.size() && s_states[global] == state;
}

bool vfs_asset_is_stubbed(uint64_t uop_hash)
{
    if (!vfs_enabled() || uop_hash == 0)
        return false;
    vfs_managed_build();
    const auto found = s_managed_hashes.find(uop_hash);
    if (found == s_managed_hashes.end())
        return false;
    return vfs_asset_state_is(
        uint32_t(found->second.first), uint32_t(found->second.second), vfs_stubbed);
}

// planned payload bytes of one entry inside a managed pack mapping, resolved
// by the entry's uop name hash. The on-disk table entry of a not-yet-
// materialized block carries Offset == 0 (and UopLoadFile skips it), but the
// plan fixes every block's byte range up front: resolving through the plan
// lets the game's index expose entries whose bytes are not there yet, so
// raw-pointer reads fault into the trap and stream on demand. Returns false
// when streaming is disabled, the base is not a managed pack mapping, the
// hash is unknown to the plan, or the entry is a hole (reserved slot with
// no bytes).
bool vfs_asset_by_hash(
    const uint8_t *mapping_base, uint64_t uop_hash, uint64_t *out_offset, uint32_t *out_size)
{
    if (!vfs_enabled() || mapping_base == nullptr || uop_hash == 0)
        return false;
    vfs_managed_build();
    const auto found = s_managed_hashes.find(uop_hash);
    if (found == s_managed_hashes.end())
        return false;
    const auto file_index = found->second.first;
    const auto asset_index = found->second.second;
    if (file_index >= s_plan.files.size() || asset_index >= s_plan.files[file_index].assets.size())
        return false;
    const auto &asset = s_plan.files[file_index].assets[asset_index];
    if (vfs_asset_is_hole(asset))
        return false; // reserved slot: genuinely no asset, keep Address == 0
    if (out_offset != nullptr)
        *out_offset = asset.file_offset + asset.ml;
    if (out_size != nullptr)
        *out_size = uint32_t(asset.ul);
    return true;
}

bool vfs_asset_entry_by_hash(uint64_t uop_hash, const char **out_owner, UopFileEntry *out_entry)
{
    if (out_entry == nullptr || !vfs_enabled() || uop_hash == 0)
        return false;
    vfs_managed_build();
    const auto found = s_managed_hashes.find(uop_hash);
    if (found == s_managed_hashes.end())
        return false;
    const auto file_index = found->second.first;
    const auto asset_index = found->second.second;
    if (file_index >= s_plan.files.size() || asset_index >= s_plan.files[file_index].assets.size())
        return false;
    const auto &file = s_plan.files[file_index];
    const auto &asset = file.assets[asset_index];
    if (vfs_asset_is_hole(asset))
        return false;
    // the entry exactly as the materialized pack's table describes the block
    // (the write path patches the same values in when the block lands)
    UopFileEntry entry;
    entry.Offset = asset.file_offset;
    entry.MetadataSize = asset.ml;
    entry.CompressedSize = uint32_t(asset.ul);
    entry.DecompressedSize = uint32_t(asset.ul);
    entry.Hash = asset.uop_hash;
    entry.Checksum = asset.mc;
    entry.Flags = uint16_t(asset.ct == 1 ? 0 : asset.ct);
    *out_entry = entry;
    if (out_owner != nullptr)
        *out_owner = file.name.c_str();
    return true;
}

#if defined(XUO_EMSC)
// ---------------------------------------------------------------------------
// punctual block fetch (web): the replacement for mapped-file block reads.
// Resolves the plan asset behind an entry hash, ranged-GETs the block
// ([ml metadata][payload]) straight from the file repo - the browser HTTP
// cache persists it across sessions - and keeps a bounded LRU of raw block
// payloads in the wasm heap. No MEMFS: a full pack never exists on disk or
// in memory, only the hot blocks the game actually reads.
// ---------------------------------------------------------------------------
struct vfs_block_lru_entry
{
    std::shared_ptr<std::vector<uint8_t>> data;
    uint64_t ticks = 0;
};
static std::mutex s_block_lru_mutex;
static std::unordered_map<uint64_t, vfs_block_lru_entry> s_block_lru;
static std::deque<uint64_t> s_block_lru_order;
static size_t s_block_lru_bytes = 0;
static size_t s_block_lru_clock = 0;
static size_t s_block_fetch_hits = 0;
static size_t s_block_fetch_miss = 0;
static size_t s_block_fetch_fails = 0;
// hashes whose size already joined the web download denominator (the LRU
// evicts and re-fetches, the expectation must register once)
static std::unordered_set<uint64_t> s_block_expect;
constexpr size_t VFS_BLOCK_LRU_CAP = 128ull << 20; // 128 MB of hot blocks

static std::shared_ptr<std::vector<uint8_t>> s_block_lru_hit(uint64_t hash)
{
    std::lock_guard<std::mutex> lock(s_block_lru_mutex);
    const auto found = s_block_lru.find(hash);
    if (found == s_block_lru.end())
        return nullptr;
    found->second.ticks = ++s_block_lru_clock;
    return found->second.data;
}

static void s_block_lru_store(uint64_t hash, std::shared_ptr<std::vector<uint8_t>> data)
{
    std::lock_guard<std::mutex> lock(s_block_lru_mutex);
    const size_t bytes = data->size();
    s_block_lru[hash] = {std::move(data), ++s_block_lru_clock};
    s_block_lru_bytes += bytes;
    while (s_block_lru_bytes > VFS_BLOCK_LRU_CAP && !s_block_lru.empty())
    {
        // small victim scan: the map is keyed by hash, the clock orders
        uint64_t victim = 0;
        uint64_t best = UINT64_MAX;
        for (const auto &entry : s_block_lru)
        {
            if (entry.second.ticks < best)
            {
                best = entry.second.ticks;
                victim = entry.first;
            }
        }
        const auto found = s_block_lru.find(victim);
        const size_t entry_bytes = found->second.data ? found->second.data->size() : 0;
        s_block_lru_bytes -= std::min(s_block_lru_bytes, entry_bytes);
        s_block_lru.erase(found);
    }
}

std::shared_ptr<std::vector<uint8_t>> vfs_asset_fetch_block_range(uint64_t uop_hash)
{
    if (!vfs_enabled() || uop_hash == 0)
        return nullptr;
    if (auto hit = s_block_lru_hit(uop_hash))
    {
        ++s_block_fetch_hits;
        return hit;
    }

    vfs_managed_build();
    const auto found = s_managed_hashes.find(uop_hash);
    if (found == s_managed_hashes.end())
        return nullptr;
    const auto file_index = found->second.first;
    const auto asset_index = found->second.second;
    if (file_index >= s_plan.files.size() || asset_index >= s_plan.files[file_index].assets.size())
        return nullptr;
    const auto &asset = s_plan.files[file_index].assets[asset_index];
    if (vfs_asset_is_hole(asset))
        return nullptr;
    if (s_params.offline || !vfs_fetch_started())
        return nullptr;

    // whole-blob assets are allowed when small (normalized pack part
    // groups fetch this way - one ranged GET of the plain blob); beyond the
    // cap the fetch would stall the caller on a giant transfer - those must
    // be normalized (proxy decompression) or prefetched instead
    if (asset.blob_offset == 0)
    {
        constexpr uint64_t VFS_WEB_WHOLE_BLOB_MAX = 32ull << 20;
        const uint64_t whole = vfs_asset_range_len(asset);
        if (whole > VFS_WEB_WHOLE_BLOB_MAX)
        {
            static std::unordered_set<uint64_t> warned;
            if (warned.insert(uop_hash).second)
            {
                LOG_ERROR(
                    "web fetch: %s is a whole-blob asset (%s, %" PRIu64
                    " bytes) - too large for a punctual fetch",
                    s_plan.files[file_index].name.c_str(),
                    asset.blob.c_str(),
                    whole);
            }
            ++s_block_fetch_fails;
            return nullptr;
        }
    }

    const uint64_t len = vfs_asset_range_len(asset);
    {
        std::lock_guard<std::mutex> lock(s_block_lru_mutex);
        // denominator: per-block sizes register once per session (re-fetches
        // of evicted blocks re-count done bytes, never the expectation)
        if (s_block_expect.insert(uop_hash).second)
            vfs_web_bytes_expect(len);
    }
    const astr_t url = s_plan.file_repo + asset.blob;
    auto bytes = std::make_shared<std::vector<uint8_t>>(size_t(len), 0);
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
        bytes.get(),
        0,
        false);
    if (!res.ok || res.bytes != len)
    {
        ++s_block_fetch_fails;
        return nullptr;
    }
    ++s_block_fetch_miss;
    s_block_lru_store(uop_hash, bytes);
    return bytes;
}

 // shared tail of the punctual/warm block paths: strip the [ml] metadata
// prefix (range assets only - whole-blob assets remote as the bare payload)
// and inflate ct == 1 blocks when `decoded`.
static std::shared_ptr<std::vector<uint8_t>> vfs_block_payload_decode(
    const std::shared_ptr<std::vector<uint8_t>> &range,
    size_t strip, uint32_t flags, uint32_t decoded_size, bool decoded)
{
    if (range == nullptr || range->size() < strip)
        return nullptr;
    const size_t payload_len = range->size() - strip;
    const uint8_t *payload = range->data() + strip;
    if (!decoded || flags != 1)
        return std::make_shared<std::vector<uint8_t>>(payload, payload + payload_len);
    auto out = std::make_shared<std::vector<uint8_t>>(decoded_size, 0);
    uLongf dst_len = out->size();
    if (uncompress(out->data(), &dst_len, reinterpret_cast<Bytef const *>(payload), uLong(payload_len)) != Z_OK)
        return nullptr;
    out->resize(size_t(dst_len));
    return out;
}

// resolve the plan asset behind a block hash (bounds + hole checks applied);
// null when the hash is unknown or reserved
static const VfsAsset *vfs_block_asset_of(uint64_t uop_hash)
{
    vfs_managed_build();
    const auto found = s_managed_hashes.find(uop_hash);
    if (found == s_managed_hashes.end())
        return nullptr;
    const auto file_index = found->second.first;
    const auto asset_index = found->second.second;
    if (file_index >= s_plan.files.size() || asset_index >= s_plan.files[file_index].assets.size())
        return nullptr;
    const auto &asset = s_plan.files[file_index].assets[asset_index];
    if (vfs_asset_is_hole(asset))
        return nullptr;
    return &asset;
}

// block payload without the [ml] metadata prefix; decoded inflates zlib
// blocks (ct == 1); stored (0) and game-decoded encodings (3 = BWT) pass
// through untouched. `block` (optional) supplies the encoding; otherwise it
// resolves from the plan asset.
std::shared_ptr<std::vector<uint8_t>> vfs_asset_fetch_block_payload(
    uint64_t uop_hash, const UopFileEntry *block, bool decoded)
{
    const auto range = vfs_asset_fetch_block_range(uop_hash);
    if (range == nullptr)
        return nullptr;
    const auto *asset = vfs_block_asset_of(uop_hash);
    if (asset == nullptr)
        return nullptr;
    const size_t ml = block != nullptr ? block->MetadataSize : size_t(asset->ml);
    const uint32_t flags = block != nullptr ? block->Flags : uint32_t(asset->ct);
    const uint32_t decoded_size = block != nullptr ? block->DecompressedSize : uint32_t(asset->ul);
    // range assets are [ml metadata][payload] windows into the pack source;
    // whole-blob assets (blob_offset == 0, e.g. art's per-tile blobs) remote
    // as the bare payload with no metadata prefix - stripping ml there
    // corrupts the stream (the arena ensure makes the same distinction)
    return vfs_block_payload_decode(
        range, asset->blob_offset != 0 ? ml : 0, flags, decoded_size, decoded);
}

// warm-only payload: LRU first, then the blob store (where the async fetch
// engine lands whole blobs) - a store hit is a local file read, never
// network. Returns null on a cold cache; callers pair that with
// vfs_asset_block_enqueue and retry once the pool delivers the blob. This is
// what render/packet-dispatch readers must use: the punctual fetch above
// transfers synchronously on the calling thread, and a cold upstream costs
// the frame loop one WAN round trip per read.
std::shared_ptr<std::vector<uint8_t>> vfs_asset_block_payload_warm(
    uint64_t uop_hash, const UopFileEntry *block, bool decoded)
{
    if (!vfs_enabled() || uop_hash == 0)
        return nullptr;
    if (auto hit = s_block_lru_hit(uop_hash))
    {
        const auto *asset = vfs_block_asset_of(uop_hash);
        if (asset == nullptr)
            return nullptr;
        const size_t ml = block != nullptr ? block->MetadataSize : size_t(asset->ml);
        const uint32_t flags = block != nullptr ? block->Flags : uint32_t(asset->ct);
        const uint32_t decoded_size = block != nullptr ? block->DecompressedSize : uint32_t(asset->ul);
        return vfs_block_payload_decode(
            hit, asset->blob_offset != 0 ? ml : 0, flags, decoded_size, decoded);
    }

    const auto *asset = vfs_block_asset_of(uop_hash);
    if (asset == nullptr || fs_path_empty(s_params.blobs_dir))
        return nullptr;
    const auto path = fs_path_join(s_params.blobs_dir, asset->blob);
    if (!fs_path_exists(path))
        return nullptr;
    std::vector<uint8_t> blob;
    if (!fs_file_read(fs_path_ascii(path), blob))
        return nullptr;
    // the store mirrors the remote tree with whole blobs; slice this asset's
    // window ([blob_offset, +len)) out of it. A short file is a partial or
    // foreign entry - treat it as cold, the store write path owns correctness
    const uint64_t len = vfs_asset_range_len(*asset);
    if (blob.size() < asset->blob_offset || uint64_t(blob.size()) - asset->blob_offset < len)
        return nullptr;
    auto bytes = std::make_shared<std::vector<uint8_t>>(
        blob.begin() + asset->blob_offset, blob.begin() + asset->blob_offset + len);
    // prime the LRU so later reads of this block stop re-reading the store
    s_block_lru_store(uop_hash, bytes);
    const size_t ml = block != nullptr ? block->MetadataSize : size_t(asset->ml);
    const uint32_t flags = block != nullptr ? block->Flags : uint32_t(asset->ct);
    const uint32_t decoded_size = block != nullptr ? block->DecompressedSize : uint32_t(asset->ul);
    return vfs_block_payload_decode(
        bytes, asset->blob_offset != 0 ? ml : 0, flags, decoded_size, decoded);
}

// ---------------------------------------------------------------------------

// background-stream the blob behind a block hash into the store: fire the
// async fetch pool at it (demand precedence; deduplicated and idempotent)
// and let the warm path above pick it up on a later call. Render-critical
// callers invoke this on every cache miss, so a blob that keeps failing
// (404, stalled upstream) is re-queued at most once per cooldown window
// instead of once per render pass.
void vfs_asset_block_enqueue(uint64_t uop_hash)
{
    if (!vfs_enabled() || uop_hash == 0 || s_params.offline || !vfs_fetch_started())
        return;
    const auto *asset = vfs_block_asset_of(uop_hash);
    if (asset == nullptr)
        return;
    static std::mutex s_enqueue_mutex;
    static std::unordered_map<astr_t, uint64_t> s_next_try;
    const uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now().time_since_epoch())
                             .count();
    {
        std::lock_guard<std::mutex> lock(s_enqueue_mutex);
        const auto found = s_next_try.find(asset->blob);
        if (found != s_next_try.end() && now < found->second)
            return;
        s_next_try[asset->blob] = now + 2000;
    }
    // the wire size is the stored/compressed transfer (the pool tolerates
    // normalized transfers that legitimately differ on web)
    const uint64_t wire = asset->ct == 0 ? uint64_t(asset->ul) : uint64_t(asset->cl);
    vfs_fetch_queue_blob(asset->blob.c_str(), wire, false, VFS_FETCH_PRIO_DEMAND);
}

#if defined(XUO_EMSC)
// ---------------------------------------------------------------------------
// stream files (web install): paced remote->disk materialization for the
// loose whole-file transfers the web subset keeps only as plan metadata
// (VfsFile::streamable - the plan carries name/URL/sizes, boot never touches
// the bytes).
//
// The legacy MUL animation set (players, humans, classic creatures plus the
// bodyconv expansion bodies) exists ONLY in anim.mul/anim2-5.mul beside
// their .idx tables: the AnimationFrame UOP packs carry just the bodies
// added after the legacy set, and the streamed web subset keeps the mul
// bytes out of the boot install. The upstream blobs are whole-stream zlib
// (anim.mul: 123MB compressed -> 194MB raw), so punctual range reads cannot
// decode a middle window, and the fixed 1GB wasm heap holds neither a
// whole-blob fetch buffer nor a mapped image. Each file therefore streams
// down in modest ranged chunks, inflates incrementally into the install
// file, and the completed file then serves bounded window reads straight
// from disk - the image is never resident.
//
// Registry shape: one state-machine instance per canonical file name. The
// chunk steps execute in the fetch pool (vfs_fetch's stream lane): a read
// miss or an ensure DEMANDS the file's chunk loop, the pool runs one paced
// chunk at a time on a worker, and every executed step re-queues itself
// until the file leaves its streaming phase. The render thread NEVER
// issues a stream transfer - vfs_stream_file_read is warm-only (a window
// read once the file is ready, null otherwise) and only its demand signal
// reaches the pool. Pacing (2MB / 120ms) is enforced pool-side through the
// job's runnable gate; demand block fetches always outrank stream chunks
// and keep a reserved worker (the stream chunk shrinks the sweep lane).
//
// A chunk failure never costs the stream its progress: the z_stream (which
// carries zlib's own 32KB sliding window), the compressed offset and the
// append-mode output handle are INSTANCE state and survive job boundaries,
// so a failed transfer just refetches its chunk. A chunk that arrives
// intact but decodes bad additionally rebuilds the inflater at a
// checkpoint (the last 32KB of inflated output plus the exact compressed
// bit position) instead of restarting the file from byte 0 - only a
// second decode failure at the same region falls back to the bounded
// from-zero restart. Restarts (<=3) and the 60s failure cooldown stay per
// instance. Stream reads never touch the shared block LRU - the returned
// window aliases the instance's own buffer.
// ---------------------------------------------------------------------------

namespace {

struct WebStreamFile
{
    enum class Phase
    {
        Probe,     // validate a previous session's output
        Streaming, // chunk loop: range GET -> inflate -> write (pool-driven)
        Ready,     // the install file is complete
        Failed     // gave up after repeated restarts (cooling down)
    };

    astr_t name;        // canonical lowercase plan name ("anim.mul")
    astr_t out_name;    // the plan's case-exact name (install path; MEMFS is case-sensitive)
    astr_t url;         // file repo + the plan entry's blob tail
    bool url_ready = false;
    bool url_missing_logged = false;
    bool stored = false; // plan asset shipped uncompressed (ct != 1): verbatim copy
    fs_path out_path;   // install_dir/<name>
    fs_path done_path;  // install_dir/cache/<name>.ready

    // everything below is guarded by `mutex`: the render thread's
    // probe/read/ensure and the pool worker's chunk steps both touch it.
    // The writer handle is closed + flushed and the marker published
    // BEFORE the phase flips to Ready under this same mutex, and readers
    // only open the install file once Ready - the no-concurrent-read/write
    // invariant on one install path holds by construction.
    mutable std::mutex mutex;
    std::condition_variable cv; // wakes a blocking ensure on phase flips
    Phase phase = Phase::Probe;
    FILE *out = nullptr; // install file being written (append semantics)
    FILE *ro = nullptr;  // install file opened for reads (Ready only)
    size_t ro_size = 0;
    z_stream zs {};
    bool zs_active = false;
    uint64_t compressed_off = 0;
    uint64_t compressed_total = 0; // from the first Content-Range answer
    uint64_t inflated_total = 0;
    uint64_t chunk_failures = 0;
    int restarts = 0;
    std::chrono::steady_clock::time_point retry_at {};

    // mid-stream resume checkpoint (the ~32KB zlib window the original
    // anim.mul design deferred): the tail of inflated output plus the
    // exact compressed bit position, captured after every good chunk. A
    // chunk that decodes bad rebuilds the inflater here instead of
    // restarting the whole file from byte 0. Between checkpoints the live
    // z_stream IS the sliding window (transfer failures resume from
    // compressed_off without any rebuild).
    std::vector<uint8_t> resume_window; // <= 32KB inflated tail
    uint64_t resume_off = 0;            // compressed byte offset to re-feed
    uint32_t resume_bits = 0;           // consumed bits of the byte at resume_off
    uint8_t resume_byte = 0;            // that partial byte's value
    uint64_t resume_inflated = 0;       // inflated_total at the checkpoint
    bool resumed = false;               // the inflater runs in resumed raw mode
    int decode_resumes = 0;             // resume attempts since the last good chunk

    // progress telemetry
    uint64_t chunks_ok = 0;
    std::chrono::steady_clock::time_point last_log {};
    std::vector<uint8_t> window; // last served read window
};

// the registry holds one instance per demanded streamable plan name (the
// legacy anim muls, facet charts, verdata, the music files, and - when a
// plan stops materializing them - the statics/texmaps/multi muls). The
// unique_ptr keeps instance addresses stable: pool workers hold a pointer
// across steps while the render thread registers new files. Instances are
// never evicted. A flat vector keeps insertion order; the cap only guards
// against pathological plan names.
constexpr size_t WEB_STREAM_FILES_MAX = 128;
std::vector<std::unique_ptr<WebStreamFile>> s_web_stream_files;
std::mutex s_web_stream_registry; // guards the vector only (not the instances)

constexpr uint64_t WEB_STREAM_CHUNK = 2u << 20;  // compressed bytes/step
constexpr size_t WEB_STREAM_READ_CAP = 4u << 20; // sanity per read
// polite-network pacing between chunks, enforced pool-side through the
// job's runnable gate (no render-thread stall to bound anymore, but the
// lane stays a citizen and leaves workers + connections to DEMAND block
// fetches): 2MB / 120ms ~= 16MB/s warm
constexpr int WEB_STREAM_PACE_MS = 120;
// how often a streaming file logs its progress line
constexpr int WEB_STREAM_LOG_MS = 5000;

WebStreamFile *web_stream_lookup(const char *name)
{
    // registration-free lookup (pool workers); caller registers on demand
    const astr_t lower = vfs_lower_ascii(name);
    if (lower.empty())
        return nullptr;
    std::lock_guard<std::mutex> lock(s_web_stream_registry);
    for (auto &instance : s_web_stream_files)
    {
        if (instance->name == lower)
            return instance.get();
    }
    return nullptr;
}

WebStreamFile *web_stream_instance(const char *name)
{
    const astr_t lower = vfs_lower_ascii(name);
    if (lower.empty())
        return nullptr;
    {
        std::lock_guard<std::mutex> lock(s_web_stream_registry);
        for (auto &instance : s_web_stream_files)
        {
            if (instance->name == lower)
                return instance.get();
        }
    }
    // register on demand, but only names the plan carries as streamable:
    // an unknown name has no URL and no bytes anywhere
    vfs_managed_build();
    if (!vfs_plan_ready())
        return nullptr;
    const auto found = s_managed_files.find(lower);
    if (found == s_managed_files.end() || !s_plan.files[found->second].streamable)
        return nullptr;
    std::lock_guard<std::mutex> lock(s_web_stream_registry);
    // re-check under the lock (two demanders can race the registration)
    for (auto &instance : s_web_stream_files)
    {
        if (instance->name == lower)
            return instance.get();
    }
    if (s_web_stream_files.size() >= WEB_STREAM_FILES_MAX)
    {
        LOG_ERROR("stream files: registry full (%zu), cannot stream %s", s_web_stream_files.size(), name);
        return nullptr;
    }
    auto s = std::make_unique<WebStreamFile>();
    s->name = lower;
    // install path keeps the plan's case-exact name: consumers open the
    // completed file by their own path spelling and MEMFS is case-sensitive
    s->out_name = s_plan.files[found->second].name;
    s->out_path = fs_path_join(s_params.install_dir, s->out_name);
    s->done_path = fs_path_join(fs_path_join(s_params.install_dir, "cache"), s->name + ".ready");
    WebStreamFile *const ptr = s.get();
    s_web_stream_files.push_back(std::move(s));
    return ptr;
}

bool web_stream_compose_url(WebStreamFile &s)
{
    if (s.url_ready)
        return !s.url.empty();
    if (!vfs_plan_ready() || s_plan.file_repo.empty())
        return false;
    vfs_managed_build();
    const auto found = s_managed_files.find(s.name);
    if (found == s_managed_files.end() || !s_plan.files[found->second].streamable)
    {
        // the plan does not carry the file (older manifest/plan): no URL,
        // the instance stays cold - retry composition on later calls, log once
        if (!s.url_missing_logged)
        {
            s.url_missing_logged = true;
            LOG_WARN("stream files: %s is not in the plan, staying cold", s.name.c_str());
        }
        return false;
    }
    const auto &file = s_plan.files[found->second];
    if (file.is_uop || file.assets.empty())
    {
        if (!s.url_missing_logged)
        {
            s.url_missing_logged = true;
            LOG_WARN("stream files: %s has no loose plan asset, staying cold", s.name.c_str());
        }
        return false;
    }
    // the fetch engine's own join: repo + the asset's blob tail
    s.url = s_plan.file_repo + file.assets.front().blob;
    // most whole-file blobs are one zlib stream (anim.mul); some loose files
    // ship stored (ct != 1, e.g. LoginLoop.mp3) and copy through verbatim
    s.stored = file.assets.front().ct != 1;
    s.url_ready = true;
    return true;
}

void web_stream_reset(WebStreamFile &s)
{
    if (s.out != nullptr)
    {
        fclose(s.out);
        s.out = nullptr;
    }
    if (s.zs_active)
    {
        inflateEnd(&s.zs);
        s.zs_active = false;
    }
    s.compressed_off = 0;
    s.inflated_total = 0;
    // the resume checkpoint dies with the stream state (a fresh stream
    // re-captures it after its first good chunk)
    s.resume_window.clear();
    s.resume_off = 0;
    s.resume_bits = 0;
    s.resume_byte = 0;
    s.resume_inflated = 0;
    s.resumed = false;
    s.decode_resumes = 0;
}

// from-zero restart, bounded: true keeps the loop alive (the next chunk
// starts over), false entered the Failed cooldown (no re-queue)
bool web_stream_restart(WebStreamFile &s)
{
    web_stream_reset(s);
    ++s.restarts;
    if (s.restarts > 3)
    {
        s.phase = WebStreamFile::Phase::Failed;
        s.retry_at = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        LOG_WARN(
            "stream files: %s failed after %d restarts, cooling down 60s",
            s.name.c_str(),
            s.restarts - 1);
        return false;
    }
    LOG_WARN("stream files: %s restarting from byte 0 (restart %d/3)", s.name.c_str(), s.restarts);
    return true;
}

// capture the resume checkpoint at the current stream position: zlib hands
// out its own 32KB sliding window (inflateGetDictionary) and reports the
// unused bits of the last taken input byte through data_type, which pins
// the exact bit position inside the fed chunk
void web_stream_capture_checkpoint(WebStreamFile &s, uint8_t last_fed_byte)
{
    uint8_t dict[32 * 1024];
    uInt dict_len = 0;
    if (inflateGetDictionary(&s.zs, dict, &dict_len) != Z_OK)
        return;
    s.resume_window.assign(dict, dict + dict_len);
    const uint32_t unused = uint32_t(s.zs.data_type) & 63u; // low bits: unused bits
    if (unused == 0)
    {
        s.resume_off = s.compressed_off;
        s.resume_bits = 0;
    }
    else
    {
        // the last byte was only partially consumed: the checkpoint sits on
        // it, and a resume primes its consumed bits before re-feeding
        s.resume_off = s.compressed_off - 1;
        s.resume_bits = 8 - unused;
        s.resume_byte = last_fed_byte;
    }
    s.resume_inflated = s.inflated_total;
}

// rebuild the inflater at the checkpoint after a chunk decoded bad: a raw
// inflate seeded with the 32KB window plus the partial input byte's bits
// continues the stream at the exact bit position. The zlib wrapper's
// adler32 validation is forfeited on a resumed stream (the marker's byte
// count stays the completion authority, as documented)
bool web_stream_resume_inflate(WebStreamFile &s)
{
    if (s.zs_active)
    {
        inflateEnd(&s.zs);
        s.zs_active = false;
    }
    memset(&s.zs, 0, sizeof(s.zs));
    if (inflateInit2(&s.zs, -15) != Z_OK) // raw deflate: resuming mid-body
        return false;
    s.zs_active = true;
    if (
        !s.resume_window.empty() &&
        inflateSetDictionary(&s.zs, s.resume_window.data(), uInt(s.resume_window.size())) != Z_OK)
    {
        return false;
    }
    if (s.resume_bits > 0 && inflatePrime(&s.zs, int(s.resume_bits), int(s.resume_byte)) != Z_OK)
    {
        return false;
    }
    // rewind the output to the checkpoint: the corrupt chunk may have
    // inflated valid-looking bytes before the bad symbol
    if (s.out != nullptr)
    {
        if (fflush(s.out) != 0 || ftruncate(fileno(s.out), off_t(s.resume_inflated)) != 0 ||
            fseek(s.out, long(s.resume_inflated), SEEK_SET) != 0)
        {
            return false;
        }
    }
    s.inflated_total = s.resume_inflated;
    s.compressed_off = s.resume_off + (s.resume_bits > 0 ? 1 : 0);
    s.resumed = true;
    return true;
}

// ONE chunk step, executed by the fetch pool's stream lane with the
// instance mutex held (never on the render thread). Returns the
// milliseconds until the file wants its next chunk (the pool re-queues the
// loop), or -1 when the file left its streaming phase (Ready/Failed/
// cooling: no job should follow). A failed step keeps the stream state
// (offsets, live zlib window, append handle) untouched, so the re-queued
// chunk resumes from compressed_off instead of restarting from byte 0.
int web_stream_step(WebStreamFile &s)
{
    if (!s.url_ready) // composed on the demand path before the job was queued
        return -1;

    std::vector<uint8_t> chunk;
    const uint64_t want = s.compressed_total != 0
        ? std::min<uint64_t>(WEB_STREAM_CHUNK, s.compressed_total - s.compressed_off)
        : WEB_STREAM_CHUNK;
    if (want == 0)
    {
        // the stream state ran past the source: restart cleanly
        return web_stream_restart(s) ? WEB_STREAM_PACE_MS : -1;
    }
    // transient-failure absorption: a range GET can fail spuriously
    // (status 0) while the fetch pool saturates the browser's connection
    // budget (observed at boot and during world entry). A failed GET is
    // retried immediately (bounded) before it counts as a failure.
    vfs_transport_result res {};
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        res = vfs_transport_active()->range(
            s.url.c_str(),
            s.url.c_str(),
            s.compressed_off,
            want,
            [](void *context, const uint8_t *data, size_t size)
            {
                static_cast<std::vector<uint8_t> *>(context)->assign(data, data + size);
                return true;
            },
            &chunk,
            0,
            false);
        if (res.ok && !chunk.empty())
        {
            break;
        }
    }
    if (res.total > 0 && s.compressed_total == 0)
    {
        s.compressed_total = uint64_t(res.total);
        // denominator: the lane's full compressed size registers once, at the
        // chunk that learned it (that chunk's own bytes are already counted
        // done, so the ratio stays monotonic from here on)
        vfs_web_bytes_expect(s.compressed_total);
    }
    if (!res.ok || chunk.empty())
    {
        // TRANSFER-FAILURE RESUME: the live z_stream (zlib's own 32KB
        // window), compressed_off and the append handle are all intact -
        // the re-queued chunk refetches from compressed_off
        ++s.chunk_failures;
        if (s.chunk_failures >= 8)
        {
            // the source keeps failing: cool down instead of hammering it
            s.chunk_failures = 0;
            s.restarts = 4; // surface as failed after the current stream resets
            web_stream_reset(s);
            s.phase = WebStreamFile::Phase::Failed;
            s.retry_at = std::chrono::steady_clock::now() + std::chrono::seconds(60);
            return -1;
        }
        return WEB_STREAM_PACE_MS;
    }
    s.chunk_failures = 0;

    if (s.out == nullptr)
    {
        fs_path_create(fs_directory(s.out_path));
        s.out = fopen(fs_path_ascii(s.out_path), "wb");
        if (s.out == nullptr)
        {
            s.phase = WebStreamFile::Phase::Failed;
            s.retry_at = std::chrono::steady_clock::now() + std::chrono::seconds(60);
            return -1;
        }
        if (!s.stored)
        {
            memset(&s.zs, 0, sizeof(s.zs));
            if (inflateInit(&s.zs) != Z_OK)
            {
                fclose(s.out);
                s.out = nullptr;
                s.phase = WebStreamFile::Phase::Failed;
                s.retry_at = std::chrono::steady_clock::now() + std::chrono::seconds(60);
                return -1;
            }
            s.zs_active = true;
        }
    }

    if (s.stored)
    {
        // stored blob: the bytes are the file, copy the chunk verbatim
        if (fwrite(chunk.data(), 1, chunk.size(), s.out) != chunk.size())
        {
            if (!web_stream_restart(s))
                return -1;
            return WEB_STREAM_PACE_MS;
        }
        s.inflated_total += chunk.size();
        s.compressed_off += chunk.size();
        if (s.compressed_total != 0 && s.compressed_off >= s.compressed_total)
        {
            const bool flushed = fflush(s.out) == 0;
            fclose(s.out);
            s.out = nullptr;
            if (!flushed)
            {
                if (!web_stream_restart(s))
                    return -1;
                return WEB_STREAM_PACE_MS;
            }
            char text[32] = {};
            snprintf(text, sizeof(text), "%llu", (unsigned long long)s.inflated_total);
            FILE *done = fopen(fs_path_ascii(s.done_path), "wb");
            if (done != nullptr)
            {
                fwrite(text, 1, strlen(text), done);
                fclose(done);
            }
            LOG_INFO("stream files: %s complete (%llu raw bytes)", s.name.c_str(), (unsigned long long)s.inflated_total);
            s.phase = WebStreamFile::Phase::Ready;
            return -1;
        }
        return WEB_STREAM_PACE_MS;
    }

    s.zs.next_in = chunk.data();
    s.zs.avail_in = uInt(chunk.size());
    std::vector<uint8_t> outbuf(256 << 10);
    bool finished = false;
    bool corrupt = false;
    while (true)
    {
        s.zs.next_out = outbuf.data();
        s.zs.avail_out = uInt(outbuf.size());
        const int rc = inflate(&s.zs, Z_NO_FLUSH);
        if (rc == Z_STREAM_END)
        {
            finished = true;
        }
        else if (rc != Z_OK && rc != Z_BUF_ERROR)
        {
            corrupt = true;
            break;
        }
        const uLong produced = outbuf.size() - s.zs.avail_out;
        if (produced != 0)
        {
            if (fwrite(outbuf.data(), 1, produced, s.out) != produced)
            {
                corrupt = true; // disk write failed: treat as a bad stream
                break;
            }
            s.inflated_total += produced;
        }
        if (finished || (s.zs.avail_in == 0 && produced == 0))
        {
            break;
        }
    }

    if (corrupt)
    {
        // A Z_DATA_ERROR carries two very different meanings here: a chunk
        // that fails to decode IN FLIGHT (its own bytes are damaged - the
        // checkpoint resume refetches exactly that region), and zlib's
        // end-of-stream Adler-32 check failing (the corruption entered the
        // stream earlier and silently decoded - the window is already
        // poisoned, no resume point can help). Only the first is worth a
        // resume attempt; a check failure restarts from byte 0 directly.
        const bool check_bad =
            s.zs.msg != nullptr && strstr(s.zs.msg, "incorrect data check") != nullptr;
        if (
            !check_bad &&
            s.decode_resumes < 1 &&
            (s.resume_off != 0 || !s.resume_window.empty()) &&
            web_stream_resume_inflate(s))
        {
            ++s.decode_resumes;
            LOG_WARN(
                "stream files: %s chunk decoded bad, resuming from compressed offset %llu "
                "(32KB window, %llu raw bytes kept)",
                s.name.c_str(),
                (unsigned long long)s.compressed_off,
                (unsigned long long)s.inflated_total);
            return WEB_STREAM_PACE_MS;
        }
        if (check_bad)
        {
            LOG_WARN(
                "stream files: %s stream check failed at compressed offset %llu, "
                "restarting clean (corruption entered upstream)",
                s.name.c_str(),
                (unsigned long long)s.compressed_off);
        }
        return web_stream_restart(s) ? WEB_STREAM_PACE_MS : -1;
    }

    if (finished)
    {
        const bool flushed = fflush(s.out) == 0;
        fclose(s.out);
        s.out = nullptr;
        inflateEnd(&s.zs);
        s.zs_active = false;
        if (!flushed)
        {
            if (!web_stream_restart(s))
                return -1;
            return WEB_STREAM_PACE_MS;
        }
        // the marker publishes the validated output across sessions (the
        // zlib end-of-stream - plus adler32 on a non-resumed stream - is
        // the only size authority)
        char text[32] = {};
        snprintf(text, sizeof(text), "%llu", (unsigned long long)s.inflated_total);
        FILE *done = fopen(fs_path_ascii(s.done_path), "wb");
        if (done != nullptr)
        {
            fwrite(text, 1, strlen(text), done);
            fclose(done);
        }
        LOG_INFO("stream files: %s complete (%llu raw bytes)", s.name.c_str(), (unsigned long long)s.inflated_total);
        s.phase = WebStreamFile::Phase::Ready;
        return -1;
    }

    // good chunk: advance the compressed position first (the checkpoint
    // pins the resume point relative to it), then pin the resume checkpoint
    s.compressed_off += chunk.size();
    web_stream_capture_checkpoint(s, chunk.back());
    s.decode_resumes = 0;
    ++s.chunks_ok;
    const auto now = std::chrono::steady_clock::now();
    if (now - s.last_log >= std::chrono::milliseconds(WEB_STREAM_LOG_MS))
    {
        s.last_log = now;
                if (s.compressed_total != 0)
        {
            LOG_INFO(
                "stream files: %s %llu%% (compressed %llu/%llu, raw %llu, chunks %llu, "
                "restarts %d, chunk-failures %llu)",
                s.name.c_str(),
                (unsigned long long)(s.compressed_off * 100 / s.compressed_total),
                (unsigned long long)s.compressed_off,
                (unsigned long long)s.compressed_total,
                (unsigned long long)s.inflated_total,
                (unsigned long long)s.chunks_ok,
                s.restarts,
                (unsigned long long)s.chunk_failures);
        }
    }
    if (s.compressed_total != 0 && s.compressed_off >= s.compressed_total)
    {
        // fed the whole source: the inflater must have seen the stream end -
        // anything else is a truncated/corrupt transfer, restart
        return web_stream_restart(s) ? WEB_STREAM_PACE_MS : -1;
    }
    return WEB_STREAM_PACE_MS;
}

size_t web_stream_file_size(FILE *fp)
{
    if (fp == nullptr || fseek(fp, 0, SEEK_END) != 0)
    {
        return 0;
    }
    const long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    return size > 0 ? size_t(size) : 0;
}
} // namespace

bool vfs_stream_file_available(const char *name)
{
    if (!vfs_enabled() || name == nullptr)
        return false;
    vfs_managed_build();
    if (!vfs_plan_ready())
        return false;
    const auto found = s_managed_files.find(vfs_lower_ascii(name));
    return found != s_managed_files.end() && s_plan.files[found->second].streamable;
}

// the marker validation shared by the demand paths (disk-only, cheap):
// flips Probe to Ready (a previous session's completed output) or to
// Streaming. Caller holds the instance mutex.
static void web_stream_probe(WebStreamFile &s)
{
    if (s.phase != WebStreamFile::Phase::Probe)
        return;
    bool done = false;
    FILE *marker = fopen(fs_path_ascii(s.done_path), "rb");
    if (marker != nullptr)
    {
        char text[32] = {};
        const size_t n = fread(text, 1, sizeof(text) - 1, marker);
        fclose(marker);
        text[n] = '\0';
        const auto expected = strtoull(text, nullptr, 10);
        FILE *fp = fopen(fs_path_ascii(s.out_path), "rb");
        if (fp != nullptr)
        {
            const size_t actual = web_stream_file_size(fp);
            fclose(fp);
            done = expected != 0 && actual == size_t(expected);
        }
    }
    s.phase = done ? WebStreamFile::Phase::Ready : WebStreamFile::Phase::Streaming;
}

const uint8_t *vfs_stream_file_read(const char *name, size_t offset, uint32_t size)
{
    if (!vfs_enabled() || s_params.offline || name == nullptr)
    {
        return nullptr;
    }
    WebStreamFile *const s = web_stream_instance(name);
    if (s == nullptr)
    {
        return nullptr;
    }

    bool demand = false;
    {
        // try_lock, never lock: a worker stepping this file holds the
        // mutex for a whole chunk (~2MB transfer), and a read miss must
        // degrade to null THIS FRAME, not stall it. A worker never holds
        // the mutex of a Ready file, so warm reads never contend.
        std::unique_lock<std::mutex> guard(s->mutex, std::try_to_lock);
        if (!guard.owns_lock())
        {
            return nullptr; // mid-step: cold for now, the consumer retries
        }
        if (s->phase == WebStreamFile::Phase::Failed)
        {
            if (std::chrono::steady_clock::now() < s->retry_at)
            {
                return nullptr;
            }
            s->restarts = 0;
            s->phase = WebStreamFile::Phase::Probe;
        }
        web_stream_probe(*s);
        if (s->phase == WebStreamFile::Phase::Streaming)
        {
            // warm-only: demand the file's chunk loop in the pool (the
            // demand tier jumps any other stream's queued chunk); the
            // enqueue happens after the unlock below
            demand = true;
        }
        else if (s->phase != WebStreamFile::Phase::Ready)
        {
            return nullptr;
        }
        if (demand)
        {
            if (!s->url_ready && !web_stream_compose_url(*s))
            {
                return nullptr; // the plan does not carry the file: stays cold
            }
        }
        else
        {
            // completed install file: serve the window straight from disk
            if (size == 0 || size > WEB_STREAM_READ_CAP)
            {
                return nullptr;
            }
            if (s->ro == nullptr)
            {
                s->ro = fopen(fs_path_ascii(s->out_path), "rb");
                if (s->ro == nullptr)
                {
                    return nullptr;
                }
                s->ro_size = web_stream_file_size(s->ro);
            }
            if (offset + size > s->ro_size)
            {
                return nullptr;
            }
            if (s->window.size() < size)
            {
                s->window.resize(size);
            }
            if (fseek(s->ro, long(offset), SEEK_SET) != 0 ||
                fread(s->window.data(), 1, size, s->ro) != size)
            {
                return nullptr;
            }
            return s->window.data();
        }
    }
    vfs_fetch_queue_stream(s->name.c_str(), VFS_FETCH_PRIO_DEMAND);
    return nullptr;
}

bool vfs_stream_file_ensure(const char *name)
{
    if (!vfs_enabled() || s_params.offline || name == nullptr)
    {
        return false;
    }
    WebStreamFile *const s = web_stream_instance(name);
    if (s == nullptr)
    {
        return false;
    }
    if (!vfs_fetch_started())
    {
        // the pool drives every chunk; before it runs (no engine yet)
        // nothing can advance - the boot order brings the engine up first
        static bool logged = false;
        if (!logged)
        {
            logged = true;
            LOG_WARN("stream files: ensure(%s) without a fetch pool, staying cold", name);
        }
        return false;
    }
    // demand the completion and WAIT for the pool to deliver it: the
    // ensure contract stays "ready when true" (boot verdata / streamed
    // index files / facet chart opens), but the transfers happen on pool
    // workers - the render thread only parks on this file's phase flip.
    // A stalled connection is bounded by the deadline (a step that fails
    // outright flips the phase and returns through the cooldown instead).
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
    while (std::chrono::steady_clock::now() < deadline)
    {
        {
            std::unique_lock<std::mutex> guard(s->mutex);
            if (s->phase == WebStreamFile::Phase::Failed)
            {
                // the demand path already gave up on this file recently; the
                // ensure caller retries after the cooldown
                if (std::chrono::steady_clock::now() < s->retry_at)
                {
                    return false;
                }
                s->restarts = 0;
                s->phase = WebStreamFile::Phase::Probe;
            }
            web_stream_probe(*s);
            if (s->phase == WebStreamFile::Phase::Ready)
            {
                return true;
            }
            if (!s->url_ready && !web_stream_compose_url(*s))
            {
                return false; // the plan does not carry the file: stays cold
            }
            if (s->phase != WebStreamFile::Phase::Streaming)
            {
                continue;
            }
        }
        vfs_fetch_queue_stream(s->name.c_str(), VFS_FETCH_PRIO_DEMAND);
        {
            std::unique_lock<std::mutex> guard(s->mutex);
            // the worker notifies on every step; the predicate re-arms the
            // demand only when a step left the file failed (cooldown)
            s->cv.wait_until(guard, deadline, [s] { return s->phase != WebStreamFile::Phase::Streaming; });
        }
    }
    LOG_WARN("stream files: ensure(%s) timed out waiting for the pool", name);
    return false;
}

// pool seam (vfs_fetch's stream lane): ONE chunk step on a worker, the
// instance mutex held for its whole duration. Returns the re-queue pace,
// or -1 when the file left its streaming phase.
int vfs_stream_pool_step(const char *name)
{
    WebStreamFile *const s = web_stream_lookup(name);
    if (s == nullptr)
        return -1;
    int delay = -1;
    {
        std::lock_guard<std::mutex> guard(s->mutex);
        if (s->phase == WebStreamFile::Phase::Streaming)
        {
            delay = web_stream_step(*s);
        }
        s->cv.notify_all(); // phase flips (and progress) wake a blocking ensure
    }
    return delay;
}

// progress surfaced: one entry per registered stream file (phase, bytes,
// totals, health). Returns the number of entries written.
int vfs_stream_file_stats(vfs_stream_file_stats_entry *out, int max)
{
    if (out == nullptr || max <= 0)
        return 0;
    std::lock_guard<std::mutex> lock(s_web_stream_registry);
    int n = 0;
    for (auto &instance : s_web_stream_files)
    {
        if (n >= max)
            break;
        WebStreamFile &s = *instance;
        std::lock_guard<std::mutex> guard(s.mutex);
        vfs_stream_file_stats_entry &e = out[n++];
        e.name = s.name.c_str();
        e.phase = int(s.phase);
        e.compressed_off = s.compressed_off;
        e.compressed_total = s.compressed_total;
        e.inflated_total = s.inflated_total;
        e.chunks_ok = s.chunks_ok;
        e.chunk_failures = s.chunk_failures;
        e.restarts = s.restarts;
        e.resumed = s.resumed;
    }
    return n;
}
#endif // XUO_EMSC

void vfs_block_fetch_stats(size_t *hits, size_t *miss, size_t *fails, size_t *lru_mb)
{
    std::lock_guard<std::mutex> lock(s_block_lru_mutex);
    if (hits != nullptr) *hits = s_block_fetch_hits;
    if (miss != nullptr) *miss = s_block_fetch_miss;
    if (fails != nullptr) *fails = s_block_fetch_fails;
    if (lru_mb != nullptr) *lru_mb = s_block_lru_bytes / 1048576;
}
#endif // XUO_EMSC

bool vfs_asset_read(uint64_t uop_hash, std::vector<uint8_t> &out)
{
    // arena builds only: materialized installs read through the mapped file
    // and its trap; the arena serves packs that never become heap files
    // (web: the sound archive)
    if (s_arena == nullptr || uop_hash == 0)
        return false;
    vfs_managed_build();
    const auto found = s_managed_hashes.find(uop_hash);
    if (found == s_managed_hashes.end())
        return false;
    const auto &file = s_plan.files[found->second.first];
    const auto &asset = file.assets[found->second.second];
    if (vfs_asset_is_hole(asset))
        return false;
    if (!s_arena->ensure(asset, true)) // blocking: fetches when not resident
        return false;
    out.resize(asset.ul, 0);
    if (asset.ul != 0 && !s_arena->read(asset, 0, out.data(), out.size()))
        return false;
    return true;
}

bool vfs_plan_for_each_asset(
    const char *pack_name, bool (*visit)(const VfsAsset &asset, void *ctx), void *ctx)
{
    if (!vfs_enabled() || pack_name == nullptr)
        return false;
    const astr_t lower = vfs_lower_ascii(pack_name);
    for (const auto &file : s_plan.files)
    {
        if (!file.is_uop)
            continue;
        if (vfs_lower_ascii(file.name.c_str()) != lower)
            continue;
        if (visit != nullptr)
        {
            for (const auto &asset : file.assets)
            {
                if (vfs_asset_is_hole(asset))
                    continue; // reserved slot: no bytes, never indexed
                if (!visit(asset, ctx))
                    break;
            }
        }
        return true;
    }
    return false;
}

bool vfs_ensure_file(const char *name, fs_path *out_path, bool force)
{
    if (!vfs_enabled() || name == nullptr || name[0] == '\0')
        return false;
    const astr_t lower = vfs_lower_ascii(name);
    for (size_t fi = 0; fi < s_plan.files.size(); ++fi)
    {
        const auto &file = s_plan.files[fi];
        if (vfs_lower_ascii(file.name.c_str()) != lower)
            continue;
        const auto path = fs_path_join(s_params.install_dir, file.name);
        if (out_path != nullptr)
            *out_path = path;
        if (!force && fs_path_exists(path))
            return true;
        if (s_arena != nullptr)
        {
            // arena backend: the pack writer ensures every asset (fetching
            // on demand) and writes the file in one pass
            return vfs_store_materialize_pack(
                s_arena, s_plan, fi, s_params.install_dir, s_params.blobs_dir);
        }
        // sparse backend: fetch every missing asset (blocking); the
        // materialization writes the file progressively
        for (size_t ai = 0; ai < file.assets.size(); ++ai)
        {
            if (vfs_asset_is_hole(file.assets[ai]))
                continue;
            if (!vfs_fetch_and_materialize_asset(uint32_t(fi), uint32_t(ai), 0, false, 0))
                return false;
        }
        return fs_path_exists(path);
    }
    return false;
}

bool vfs_file_is_stubbed(const uint8_t *base)
{
    if (!vfs_enabled() || base == nullptr)
        return false;
    const VfsPlan *plan = nullptr;
    size_t file_index = 0;
    if (!vfs_trap_file_of(base, &plan, &file_index) || plan != &s_plan)
        return false;
    // loose files carry a single whole-file asset (range-resolved); pack
    // entries are marked through vfs_asset_is_stubbed instead
    return vfs_asset_state_is(uint32_t(file_index), 0, vfs_stubbed);
}

size_t vfs_file_state_base(const uint8_t *mapping_base)
{
    if (!vfs_enabled() || mapping_base == nullptr || !vfs_plan_ready())
        return SIZE_MAX;
    const VfsPlan *plan = nullptr;
    size_t file_index = 0;
    if (!vfs_trap_file_of(mapping_base, &plan, &file_index) || plan != &s_plan ||
        file_index >= s_plan.files.size())
        return SIZE_MAX;
    return vfs_state_base_index(s_plan, file_index);
}

bool vfs_asset_stubbed_at(size_t base, size_t asset_index)
{
    if (base == SIZE_MAX)
        return false;
    std::lock_guard<std::mutex> lock(s_state_mutex);
    return base + asset_index < s_states.size() && s_states[base + asset_index] == vfs_stubbed;
}

// ---------------------------------------------------------------------------
// params
// ---------------------------------------------------------------------------

// true when the product manifest path was not given explicitly and was
// defaulted into the blob store (an explicit --product file always wins over
// a re-download). Decided once per vfs_set_params: the dirs-ready path
// resolves the file name in place, which would otherwise lose the original
// "was it explicit?" answer between boots.
static bool vfs_product_file_defaulted = false;

void vfs_set_params(const vfs_params &params)
{
    s_params = params;
    s_plan_loaded = false;
    s_managed_built = false;                // the registry follows the (new) plan
    s_states.clear();                       // runtime state binds to the previous params
    vfs_trap_set_traps(s_params.traps);     // escape hatch (--streaming-trap=off)
    vfs_product_file_defaulted = fs_path_empty(s_params.product_file);
    if (fs_path_some(s_params.state_dir))
    {
        fs_path_create(s_params.state_dir);
    }
}

const vfs_params &vfs_get_params()
{
    return s_params;
}

bool vfs_enabled()
{
    return s_params.enabled;
}

const char *vfs_install_path()
{
    return fs_path_ascii(s_params.install_dir);
}

// dirs only (the product manifest may still be missing when boot fetches it)
static bool vfs_params_dirs_ready()
{
    if (fs_path_empty(s_params.install_dir))
    {
        LOG_ERROR("vfs: no install dir configured");
        return false;
    }
    if (fs_path_empty(s_params.blobs_dir))
    {
        LOG_ERROR("vfs: no blob store configured");
        return false;
    }
    if (fs_path_empty(s_params.state_dir))
    {
        s_params.state_dir = fs_path_join(s_params.install_dir, "cache", "state");
    }
    fs_path_create(s_params.state_dir);
    if (fs_path_empty(s_params.product_file))
    {
        s_params.product_file = fs_path_join(s_params.blobs_dir, VFS_PRODUCT_FILE_DEFAULT);
        vfs_product_file_defaulted = true;
    }
    else if (
        !fs_path_exists(s_params.product_file) &&
        strchr(fs_path_ascii(s_params.product_file), '/') == nullptr)
    {
        // a bare name resolves inside the blob store; anything with a path
        // separator is taken as-is (the resolution stays idempotent: boot
        // runs it once per stage)
        s_params.product_file = fs_path_join(s_params.blobs_dir, s_params.product_file);
    }
    if (s_params.product_url.empty())
    {
        s_params.product_url = VFS_PRODUCT_URL_DEFAULT;
    }
    return true;
}

// applies defaults and resolves the product manifest location
static bool vfs_params_ready()
{
    if (!vfs_params_dirs_ready())
        return false;
    if (!fs_path_exists(s_params.product_file))
    {
        LOG_ERROR("vfs: product manifest not found: %s", fs_path_ascii(s_params.product_file));
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// plan lifecycle
// ---------------------------------------------------------------------------

// the product serial is the plan's fingerprint: a persisted plan is only used
// when the product manifest still carries the same serial
static bool vfs_product_serial_data(const uint8_t *data, size_t size, uint64_t *out_serial)
{
    // format dispatch: official XML carries the serial attribute; an aops
    // patchlist fingerprints itself (any stable content hash works - the
    // serial is only ever compared against itself)
    if (size < 9 || memcmp(data, "<MythicMFT", 9) != 0)
    {
        *out_serial = vfs_aops_serial(data, size);
        return size != 0;
    }
    tinyxml2::XMLDocument doc;
    doc.Parse(reinterpret_cast<const char *>(data), size);
    auto root = doc.FirstChildElement("MythicMFT");
    if (!root)
        return false;
    auto product = root->FirstChildElement("product");
    if (!product)
        return false;
    *out_serial = product->Unsigned64Attribute("serial", 0);
    return true;
}

static bool vfs_product_serial(const fs_path &product_file, uint64_t *out_serial)
{
    std::vector<uint8_t> data;
    if (!fs_file_read(fs_path_ascii(product_file), data))
        return false;
    return vfs_product_serial_data(data.data(), data.size(), out_serial);
}

// update check: how far behind the persisted plan is, in one product
// download. Memory only - the cached manifest and the plan stay untouched
// (the next boot applies what this only reports)
bool vfs_update_check(vfs_update_info *out)
{
    if (out == nullptr)
        return false;
    *out = vfs_update_info();
    if (!vfs_params_dirs_ready())
        return false;
    http_init(); // no boot ran: this owns the layer's lazy setup (idempotent)

    VfsPlan plan;
    out->plan_found = vfs_plan_load(fs_path_join(s_params.state_dir, "plan.bin"), plan);
    if (out->plan_found)
    {
        out->local_serial = plan.product_serial;
        // a subset change rebuilds the plan at boot even on the same serial
        out->update_available = plan.subset != uint8_t(s_params.subset);
    }

    std::vector<uint8_t> data;
    if (!http_get_binary(s_params.product_url.c_str(), data))
    {
        LOG_WARN("vfs: update check: could not download the product manifest");
        return false;
    }
    if (!vfs_product_serial_data(data.data(), data.size(), &out->remote_serial))
    {
        LOG_WARN("vfs: update check: malformed product manifest");
        return false;
    }
    out->update_available = out->update_available || !out->plan_found ||
                            out->remote_serial != out->local_serial;
    if (out->update_available)
        LOG_INFO(
            "update: available (plan %" PRIu64 ", product %" PRIu64 ")",
            out->local_serial,
            out->remote_serial);
    else
        LOG_INFO("update: the install is up to date (serial %" PRIu64 ")", out->remote_serial);
    return true;
}

bool vfs_plan_rebuild(VfsPlan &out_plan)
{
    if (!vfs_params_ready())
        return false;
    vfs_manifest_params params;
    params.product_file = s_params.product_file;
    params.product_url = s_params.product_url;
    params.blobs_dir = s_params.blobs_dir;
    if (!vfs_manifest_build(params, out_plan))
        return false;
    // every rebuild (boot or CLI) honors the params' subset view
    vfs_manifest_apply_subset(out_plan, s_params.subset);
    // whole-file range assets that are UOP archives become per-block range
    // packs (HTTP Range streaming); offline rebuilds keep whole files
    if (!s_params.offline)
    {
        vfs_manifest_refine_ranges(out_plan, s_params.blobs_dir, out_plan.file_repo, 512 * 1024);
    }
    if (!vfs_plan_save(fs_path_join(s_params.state_dir, "plan.bin"), out_plan))
        return false;
    s_plan = out_plan;
    s_plan_loaded = true;
    return true;
}

bool vfs_plan_ensure(VfsPlan &out_plan)
{
    if (!vfs_params_ready())
        return false;
    if (s_plan_loaded)
    {
        out_plan = s_plan;
        return true;
    }

    const auto plan_path = fs_path_join(s_params.state_dir, "plan.bin");
    uint64_t serial = 0;
    if (vfs_plan_load(plan_path, out_plan) && vfs_product_serial(s_params.product_file, &serial))
    {
        if (serial == out_plan.product_serial)
        {
            // the cached plan honors the params' subset view too
            vfs_manifest_apply_subset(out_plan, s_params.subset);
            s_plan = out_plan;
            s_plan_loaded = true;
            LOG_DEBUG("plan loaded: %s", fs_path_ascii(plan_path));
            return true;
        }
        LOG_INFO(
            "plan serial drift: product %" PRIu64 " != plan %" PRIu64,
            serial,
            out_plan.product_serial);
    }
    return vfs_plan_rebuild(out_plan);
}

// ---------------------------------------------------------------------------
// state db
// ---------------------------------------------------------------------------

static bool vfs_state_open(const VfsPlan &plan, std::vector<uint8_t> &states, vfs_journal &journal)
{
    const size_t count = vfs_state_asset_count(plan);
    vfs_state_load(fs_path_join(s_params.state_dir, "state.bin"), count, states);

    std::vector<vfs_journal_record> records;
    vfs_journal_replay(fs_path_join(s_params.state_dir, "journal.log"), records);
    vfs_state_apply_journal(plan, records, states);

    vfs_reconcile_stats reconcile;
    vfs_state_reconcile(plan, s_params.install_dir, states, &reconcile, s_params.blobs_dir);
    if (reconcile.assets_repaired)
    {
        LOG_INFO(
            "reconcile: %zu of %zu assets downgraded to pending",
            reconcile.assets_repaired,
            reconcile.assets_checked);
    }

    if (!vfs_journal_open(journal, fs_path_join(s_params.state_dir, "journal.log")))
    {
        LOG_ERROR("could not open journal");
        return false;
    }
    return true;
}

static bool
vfs_state_close(const VfsPlan &plan, const std::vector<uint8_t> &states, vfs_journal &journal)
{
    vfs_journal_close(journal);
    // the save is the fold point: it flushes state.bin to stable storage
    // BEFORE the journal is truncated, so a crash mid-rotation loses nothing
    // (every record it would drop is already durable in state.bin)
    if (!vfs_state_save(fs_path_join(s_params.state_dir, "state.bin"), states))
        return false;
    // WAL rotation: state.bin now supersedes every journal record, so a clean
    // cycle restarts the log empty and it cannot grow without bound
    const auto journal_path = fs_path_join(s_params.state_dir, "journal.log");
    FILE *fp = fopen(fs_path_ascii(journal_path), "wb");
    if (fp)
        fclose(fp);
    else
        LOG_WARN("could not rotate journal: %s", fs_path_ascii(journal_path));
    return true;
}

// closes the resources boot opens before the background tier runs; null-safe
// (early failures leave them closed). Shared by shutdown and the T0-failure
// path so a failed boot cannot leave the journal or recorder dangling.
static void vfs_runtime_close()
{
    {
        std::unique_lock<std::mutex> lock(s_state_mutex);
        vfs_journal_close(s_journal);
    }
    delete s_arena; // arena mode only (null otherwise); background is joined by now
    s_arena = nullptr;
    vfs_record_close();
}

// arena residency probe (tests + web diagnostics): the tracked/resident
// granule counts of the params-selected backend; zeros when the sparse
// backend is active or no boot holds an arena
void vfs_arena_residency(size_t *out_granules, size_t *out_resident)
{
    if (out_granules != nullptr)
        *out_granules = 0;
    if (out_resident != nullptr)
        *out_resident = 0;
    std::unique_lock<std::mutex> lock(s_state_mutex);
    if (s_arena == nullptr)
        return;
    const vfs_arena_stats stats = s_arena->stats();
    if (out_granules != nullptr)
        *out_granules = stats.granules;
    if (out_resident != nullptr)
        *out_resident = stats.resident;
}

// ---------------------------------------------------------------------------
// batch operations
// ---------------------------------------------------------------------------

bool vfs_materialize(vfs_materialize_stats *stats)
{
    VfsPlan plan;
    if (!vfs_plan_ensure(plan))
        return false;
    fs_path_create(s_params.install_dir);
    fs_path_create(s_params.state_dir);

    std::vector<uint8_t> states;
    vfs_journal journal;
    if (!vfs_state_open(plan, states, journal))
        return false;

    vfs_store_params store_params;
    store_params.blobs_dir = s_params.blobs_dir;
    store_params.install_dir = s_params.install_dir;
    store_params.state_dir = s_params.state_dir;
    const bool ok = vfs_store_materialize(plan, store_params, states, &journal, stats);

    vfs_state_close(plan, states, journal);
    return ok;
}

int vfs_verify(vfs_verify_result *out)
{
    VfsPlan plan;
    if (!vfs_plan_ensure(plan))
        return -1;
    vfs_store_params store_params;
    store_params.blobs_dir = s_params.blobs_dir;
    store_params.install_dir = s_params.install_dir;
    // stub-aware: the state DB decides which mapped bytes are
    // placeholders, which report missing-block instead of corrupt
    std::vector<uint8_t> states;
    vfs_state_load(
        fs_path_join(s_params.state_dir, "state.bin"), vfs_state_asset_count(plan), states);
    std::vector<vfs_journal_record> records;
    vfs_journal_replay(fs_path_join(s_params.state_dir, "journal.log"), records);
    vfs_state_apply_journal(plan, records, states);
    return vfs_store_verify(plan, store_params, out, &states);
}

void vfs_stats(VfsStats *out)
{
    VfsStats stats;
    VfsPlan plan;
    if (out && vfs_plan_ensure(plan))
    {
        std::vector<uint8_t> states;
        vfs_state_load(
            fs_path_join(s_params.state_dir, "state.bin"), vfs_state_asset_count(plan), states);
        std::vector<vfs_journal_record> records;
        vfs_journal_replay(fs_path_join(s_params.state_dir, "journal.log"), records);
        vfs_state_apply_journal(plan, records, states);
        // arena mode: no files exist to reconcile against - the state is
        // the boot-owned residency record, not an install-dir mirror
        if (s_params.store_backend != vfs_store_backend_arena)
            vfs_state_reconcile(plan, s_params.install_dir, states, nullptr, s_params.blobs_dir);

        size_t gi = 0;
        for (const auto &file : plan.files)
        {
            ++stats.files_total;
            stats.bytes_total += file.size;
            bool complete = !file.assets.empty();
            for (const auto &asset : file.assets)
            {
                if (gi < states.size() && states[gi] == vfs_fetched)
                {
                    stats.bytes_done += vfs_asset_block_size(asset);
                }
                else
                {
                    complete = false;
                }
                ++gi;
            }
            if (complete)
                ++stats.files_done;
        }
    }
    stats.online = !s_params.offline && vfs_fetch_started();
    // faults answered with a placeholder payload (zerolat); per-asset
    // stub reports stay on vfs_stub_count()
    vfs_trap_stats trap_stats;
    vfs_trap_stats_get(&trap_stats);
    stats.stubs_served = trap_stats.stub_served;
    if (out)
        *out = stats;
}

// HUD snapshot: the same aggregate over the boot-owned in-memory state
// - no state.bin reload, no reconcile (which maps every pack), so the game
// loop can afford it at a few Hz
void vfs_hud_stats(VfsStats *out)
{
    VfsStats stats;
    if (out == nullptr || !vfs_enabled())
        return;
    {
        std::unique_lock<std::mutex> lock(s_state_mutex);
        size_t gi = 0;
        for (const auto &file : s_plan.files)
        {
            ++stats.files_total;
            stats.bytes_total += file.size;
            bool complete = !file.assets.empty();
            for (const auto &asset : file.assets)
            {
                const uint8_t state = gi < s_states.size() ? s_states[gi] : static_cast<uint8_t>(vfs_pending);
                if (state == vfs_fetched)
                {
                    stats.bytes_done += vfs_asset_block_size(asset);
                }
                else
                {
                    complete = false;
                    if (state == vfs_stubbed)
                        ++stats.stubs_active;
                }
                ++gi;
            }
            if (complete)
                ++stats.files_done;
        }
    }
    stats.online = !s_params.offline && vfs_fetch_started();
    vfs_trap_stats trap_stats;
    vfs_trap_stats_get(&trap_stats);
    stats.stubs_served = trap_stats.stub_served;
    *out = stats;
}

// ---------------------------------------------------------------------------
// boot
// ---------------------------------------------------------------------------

// one asset the boot pipeline may need: its fetch identity (URL tail + wire
// size) and its write identity (file/asset indexes for the store)
struct vfs_boot_item
{
    size_t file_index = 0;
    size_t asset_index = 0;
    size_t global_index = 0;
    astr_t tail;           // blob store / URL tail (mft_entry_remote_path form)
    uint64_t expected = 0; // exact on-wire size (0: unknown)
    uint8_t tier = vfs_tier_t2;
    astr_t name; // for failure logs ("tiledata.mul" / pack name)
};

// collects every not-yet-fetched, non-hole asset of `tier`; `fetch_list`
// gets the ones whose blob is not in the store yet (the fetch list).
// `include_fetched` collects the tier's fetched entries too (arena boot:
// the warm-boot residency contract re-ensures T0 at every start)
static void vfs_boot_collect(
    const VfsPlan &plan,
    uint8_t tier,
    std::vector<vfs_boot_item> *items,
    std::vector<vfs_boot_item> *fetch_list,
    size_t *holes,
    bool include_fetched = false)
{
    size_t gi = 0;
    for (size_t fi = 0; fi < plan.files.size(); ++fi)
    {
        const auto &file = plan.files[fi];
        if (file.streamable)
        {
            // subset-retained metadata: the bytes arrive through the
            // stream-file lane, never a boot fetch pass (regardless of the
            // tier the manifest assigned). gi still advances: it is the
            // plan-global asset index and the state DB stays aligned.
            gi += file.assets.size();
            continue;
        }
        for (size_t ai = 0; ai < file.assets.size(); ++ai, ++gi)
        {
            const auto &asset = file.assets[ai];
            if (vfs_asset_is_hole(asset))
            {
                if (holes)
                    ++*holes;
                continue;
            }
            if (asset.tier != tier)
                continue;
            vfs_boot_item item;
            item.file_index = fi;
            item.asset_index = ai;
            item.global_index = gi;
            item.tail = asset.blob;
            item.expected = asset.ct == 0 ? uint64_t(asset.ul) : uint64_t(asset.cl);
            item.tier = tier;
            item.name = vfs_asset_is_loose(asset) ? asset.name : file.name;
            if (items && gi < s_states.size() && (include_fetched || s_states[gi] != vfs_fetched))
                items->push_back(item);
            if (fetch_list && !fs_path_exists(fs_path_join(s_params.blobs_dir, asset.blob)))
                fetch_list->push_back(item);
        }
    }
}

static void vfs_boot_items_to_tails(
    const std::vector<vfs_boot_item> &items,
    std::vector<astr_t> *tails,
    std::vector<uint64_t> *sizes)
{
    if (!tails)
        return;
    tails->reserve(items.size());
    for (const auto &item : items)
    {
        tails->push_back(item.tail);
        if (sizes)
            sizes->push_back(item.expected);
    }
}

// the embedded (recorded) manifest orders the T1 tier; assets it does
// not mention keep plan order after the recorded ones. Stale entries only
// reorder - they can never break correctness.
static void vfs_boot_order_t1(const VfsPlan &plan, std::vector<vfs_boot_item> *items)
{
    const auto &embedded = vfs_manifest_embedded_table();
    if (embedded.empty() || !items)
        return;
    std::map<uint64_t, size_t> order;
    for (size_t i = 0; i < embedded.size(); ++i)
        order[embedded[i].uop_hash] = i;
    std::stable_sort(
        items->begin(),
        items->end(),
        [&plan, &order](const vfs_boot_item &a, const vfs_boot_item &b)
        {
            const auto &fa = plan.files[a.file_index].assets[a.asset_index];
            const auto &fb = plan.files[b.file_index].assets[b.asset_index];
            const auto ia = order.find(fa.uop_hash);
            const auto ib = order.find(fb.uop_hash);
            const size_t pa = ia != order.end() ? ia->second : order.size() + a.global_index;
            const size_t pb = ib != order.end() ? ib->second : order.size() + b.global_index;
            return pa < pb;
        });
}

// update wave: appends one diff-changed on-demand asset (T2/T3) to the
// background tier so an update converges behind the game instead of
// leaving holes for the first access to trip over. T0 re-fetched blocking
// in the boot thread and T1 is already in the list - this carries the rest
static void vfs_boot_wave_append(
    const VfsPlan &plan,
    const vfs_plan_index &pos,
    std::vector<vfs_boot_item> *wave)
{
    if (pos.file_index >= plan.files.size())
        return;
    const auto &file = plan.files[pos.file_index];
    if (pos.asset_index >= file.assets.size())
        return;
    const auto &asset = file.assets[pos.asset_index];
    if (vfs_asset_is_hole(asset) || asset.tier < vfs_tier_t2)
        return; // holes stream on access; T0/T1 are handled by their tiers
    vfs_boot_item item;
    item.file_index = pos.file_index;
    item.asset_index = pos.asset_index;
    item.global_index = vfs_state_base_index(plan, pos.file_index) + pos.asset_index;
    item.tail = asset.blob;
    item.expected = asset.ct == 0 ? uint64_t(asset.ul) : uint64_t(asset.cl);
    item.tier = asset.tier;
    item.name = vfs_asset_is_loose(asset) ? asset.name : file.name;
    wave->push_back(item);
}

// materializes the given items through the shared write path (subset run); a
// null `items` materializes everything reachable from the store
static bool vfs_boot_materialize(
    const VfsPlan &plan,
    const std::vector<vfs_boot_item> *items,
    vfs_materialize_stats *stats,
    bool quiet = false,
    bool record = false)
{
    std::vector<vfs_store_item> store_items;
    if (items != nullptr)
    {
        store_items.reserve(items->size());
        for (const auto &item : *items)
            store_items.push_back({ uint32_t(item.file_index), uint32_t(item.asset_index) });
    }

    vfs_store_params store_params;
    store_params.blobs_dir = s_params.blobs_dir;
    store_params.install_dir = s_params.install_dir;
    store_params.state_dir = s_params.state_dir;
    store_params.quiet = quiet;
    store_params.record = record;

    std::unique_lock<std::mutex> lock(s_state_mutex);
    const std::vector<vfs_store_item> *list = items != nullptr ? &store_items : nullptr;
    return vfs_store_materialize(plan, store_params, s_states, &s_journal, stats, list);
}

// arena counterpart: makes the granules resident instead of
// writing files, mirroring the result into the boot state DB (and journal)
// so stats/journal keep their meaning. A stub-filled granule (params
// stub_on_miss) marks the 'B' record; a failure leaves the asset pending
// for the next boot (retry there). `force` re-ensures assets the state DB
// already reports fetched - the warm-boot T0 contract: the recreated arena
// is empty no matter what state.bin says, and a forced ensure that fails
// demotes the stale advisory byte (journal 'R') so the next-boot retry
// stands.
//
// Lock scope (review): s_state_mutex covers only the per-item snapshots and
// commits - the blocking ensure() calls run with NO lock held (they may wait
// out a fetch), so stats/map snapshots never stall on granule loads. The
// chunk's missing blobs are batched up front instead (the fetch engine
// dedups against anything already in flight), which keeps the unguarded
// window down to local store reads.
static bool vfs_boot_arena_ensure(
    const VfsPlan &plan,
    const std::vector<vfs_boot_item> *items,
    vfs_store_arena *arena,
    bool force = false)
{
    std::vector<vfs_boot_item> all;
    if (items == nullptr)
    {
        size_t gi = 0;
        for (size_t fi = 0; fi < plan.files.size(); ++fi)
        {
            if (plan.files[fi].streamable)
            {
                // subset-retained metadata: bytes only via the on-demand lane
                gi += plan.files[fi].assets.size();
                continue;
            }
            for (size_t ai = 0; ai < plan.files[fi].assets.size(); ++ai, ++gi)
            {
                const auto &asset = plan.files[fi].assets[ai];
                vfs_boot_item item;
                item.file_index = fi;
                item.asset_index = ai;
                item.global_index = gi;
                item.tail = asset.blob;
                item.expected = asset.ct == 0 ? uint64_t(asset.ul) : uint64_t(asset.cl);
                item.name = vfs_asset_is_loose(asset) ? asset.name : plan.files[fi].name;
                all.push_back(item);
            }
        }
        items = &all;
    }

    // fetch pass first (online only): every missing blob of the chunk is
    // queued in one batch, so the per-item ensures below resolve from the
    // store instead of serializing network round-trips
    if (!s_params.offline && vfs_fetch_started())
    {
        std::vector<astr_t> tails;
        std::vector<uint64_t> sizes;
        for (const auto &item : *items)
        {
            const auto &asset = plan.files[item.file_index].assets[item.asset_index];
            if (vfs_asset_is_hole(asset) || asset.blob_offset != 0 ||
                fs_path_exists(fs_path_join(s_params.blobs_dir, asset.blob)))
                continue; // holes have no blob; refined blocks stream on
                          // ensure (their source is never a stored blob)
            tails.push_back(item.tail);
            sizes.push_back(item.expected);
        }
        vfs_fetch_batch(tails, sizes, 1, false, nullptr);
    }

    bool ok = true;
#if defined(XUO_EMSC)
    size_t done = 0;
    const size_t total = items->size();
    const bool full_pass = all.size() > 0 && items == &all; // full convergence
#endif
    for (const auto &item : *items)
    {
#if defined(XUO_EMSC)
        if (full_pass && (++done & 1023) == 0)
            LOG_INFO("convergence: %zu of %zu assets", done, total);
#endif
        const VfsAsset *asset = nullptr;
        uint8_t before = vfs_pending;
        {
            std::unique_lock<std::mutex> lock(s_state_mutex);
            if (item.global_index >= s_states.size())
                continue;
            const auto &plan_asset = plan.files[item.file_index].assets[item.asset_index];
            before = s_states[item.global_index];
            if (vfs_asset_is_hole(plan_asset))
            {
                // reserved entry: no blob exists - the granule slot is all we
                // owe (fetched, never journalled)
                if (before == vfs_pending)
                    s_states[item.global_index] = vfs_fetched;
                continue;
            }
            if (before == vfs_fetched && !force)
                continue;
            asset = &plan_asset;
        }
        // no state lock held here: the blocking ensure may wait out a fetch
        if (!arena->ensure(*asset, true))
        {
            ok = false; // stays missing: retried on the next boot
            if (before != vfs_pending)
            {
                // forced re-ensure (warm boot): the granule is not resident,
                // so the advisory byte must not keep claiming it is
                std::unique_lock<std::mutex> lock(s_state_mutex);
                s_states[item.global_index] = vfs_pending;
                vfs_journal_mark(
                    s_journal, { 'R', uint32_t(item.file_index), uint32_t(item.asset_index) });
            }
            continue;
        }
        std::unique_lock<std::mutex> lock(s_state_mutex);
        s_states[item.global_index] = vfs_fetched;
        vfs_journal_mark(
            s_journal, { 'F', uint32_t(item.file_index), uint32_t(item.asset_index) });
    }
    return ok;
}

// atomic file replace: the payload lands in <path>.tmp first and is renamed
// over the target, so a crash mid-write can never truncate an existing file
static bool vfs_file_write_atomic(const char *path, const std::vector<uint8_t> &data)
{
    const astr_t tmp = astr_t(path) + ".tmp";
    if (!fs_file_write(tmp.c_str(), data))
        return false;
#if defined(XUO_WINDOWS)
    if (MoveFileExA(tmp.c_str(), path, MOVEFILE_REPLACE_EXISTING))
        return true;
#else
    if (rename(tmp.c_str(), path) == 0)
        return true;
#endif
    remove(tmp.c_str());
    return false;
}

// downloads the product manifest into the blob store. An
// online boot refreshes it EVERY boot (it is ~1.6 KB) so serial drift is
// seen on the next start; a failure falls back to the cached copy with a
// warning (never fatal when the cache suffices). A local --product file
// always wins, and offline runs only download when the file is missing.
static bool vfs_boot_product_fetch()
{
    // --streaming-no-update: the cached manifest is the truth (a missing
    // one still downloads - a first boot cannot plan without it)
    if (!s_params.update_check && fs_path_exists(s_params.product_file))
        return true;
    if (!vfs_product_file_defaulted && fs_path_exists(s_params.product_file))
        return true; // explicit local product: the user owns staleness
    if (fs_path_exists(s_params.product_file) && s_params.offline)
        return true; // offline: the cached manifest is the truth
    if (s_params.offline)
    {
        LOG_ERROR("vfs: offline and no product manifest: %s", fs_path_ascii(s_params.product_file));
        return false;
    }
    LOG_INFO("fetching product manifest: %s", s_params.product_url.c_str());
    std::vector<uint8_t> data;
    if (http_get_binary(s_params.product_url.c_str(), data))
    {
        // aops roots are accepted bare ("https://host/fc"): when the fetch
        // came back as neither manifest format it is likely a site landing
        // page - retry once at the root's patchlist
        const size_t len = s_params.product_url.size();
        const bool looks_patchlist =
            len >= 9 && s_params.product_url.compare(len - 9, 9, "/patchlist") == 0;
        if (!vfs_aops_sniff_patchlist(data.data(), data.size()) &&
            (data.size() < 9 || memcmp(data.data(), "<MythicMFT", 9) != 0) && !looks_patchlist)
        {
            astr_t retry = s_params.product_url;
            while (!retry.empty() && retry.back() == '/')
                retry.pop_back();
            retry += "/patchlist";
            LOG_INFO("product manifest unrecognized: retrying at %s", retry.c_str());
            std::vector<uint8_t> retry_data;
            if (http_get_binary(retry.c_str(), retry_data) &&
                (vfs_aops_sniff_patchlist(retry_data.data(), retry_data.size()) ||
                    (retry_data.size() >= 9 &&
                        memcmp(retry_data.data(), "<MythicMFT", 9) == 0)))
            {
                s_params.product_url = retry;
                data = std::move(retry_data);
            }
            else
            {
                LOG_WARN("vfs: %s is not a product manifest either", retry.c_str());
            }
        }
    }
    if (!data.empty() && !fs_path_empty(s_params.blobs_dir))
    {
        fs_path_create(s_params.blobs_dir);
        if (vfs_file_write_atomic(fs_path_ascii(s_params.product_file), data))
            return true;
        LOG_ERROR("vfs: could not store the product manifest");
    }
    else
    {
        LOG_WARN("vfs: could not download the product manifest");
    }
    if (fs_path_exists(s_params.product_file))
    {
        LOG_WARN(
            "vfs: using the cached product manifest: %s", fs_path_ascii(s_params.product_file));
        return true;
    }
    LOG_ERROR("vfs: no product manifest available: %s", fs_path_ascii(s_params.product_file));
    return false;
}

// update flow: the store mirrors the remote URL tree, so manifests
// whose content changed upstream would be shadowed by the cached copies on
// the next plan build. A serial drift drops every cached manifest (*.mft) -
// they are small and re-downloadable by definition; part blobs stay (they
// are content-addressed; the plan diff decides which re-download). Online
// only: an offline boot could not replace what it deletes.
static void vfs_purge_cached_manifests()
{
    std::vector<fs_path> files;
    fs_path_list(s_params.blobs_dir, files);
    size_t purged = 0;
    for (const auto &file : files)
    {
        const astr_t name(fs_path_ascii(file));
        const size_t len = name.size();
        if (len < 4 || strcasecmp(name.c_str() + len - 4, ".mft") != 0)
            continue;
        fs_del(file);
        ++purged;
    }
    if (purged != 0)
        LOG_INFO("update: purged %zu cached manifests for re-download", purged);
}

// plan acquisition for boot: persisted plan when the product serial AND the
// subset match, (re)build otherwise - online through the HTTP manifest seam,
// offline from the store. On a drift the new plan is diffed against the
// persisted one and the per-asset changes are returned so boot
// can reset exactly what changed after its state load. Sets the fetch
// engine's repos either way.
static bool vfs_boot_plan(VfsPlan &out_plan, vfs_plan_diff_result *out_diff, bool *out_drifted)
{
    if (out_diff)
        *out_diff = vfs_plan_diff_result();
    if (out_drifted)
        *out_drifted = false;
    if (!vfs_params_dirs_ready())
        return false;

    if (!vfs_boot_product_fetch())
        return false;

    const auto plan_path = fs_path_join(s_params.state_dir, "plan.bin");
    VfsPlan persisted;
    uint64_t serial = 0;
    if (vfs_plan_load(plan_path, persisted) && vfs_product_serial(s_params.product_file, &serial))
    {
        const uint8_t persisted_subset = persisted.subset;
        // the persisted plan is reused only when BOTH the product serial
        // and the subset match the params. A stored view that differs from
        // the params subset is drift in BOTH directions (web->none and
        // none->web): the params view must win, so the plan rebuilds and the
        // resulting diff/reset runs like any serial change
        if (serial == persisted.product_serial && persisted_subset == uint8_t(s_params.subset))
        {
            // honor the params' subset view over the persisted plan
            vfs_manifest_apply_subset(persisted, s_params.subset);
            s_plan = persisted;
            s_plan_loaded = true;
            LOG_DEBUG("plan loaded: %s", fs_path_ascii(plan_path));
            vfs_fetch_set_repos(persisted.manifest_repo.c_str(), persisted.file_repo.c_str());
            out_plan = persisted;
            return true;
        }
        if (serial != persisted.product_serial)
        {
            LOG_INFO(
                "plan serial drift: product %" PRIu64 " != plan %" PRIu64,
                serial,
                persisted.product_serial);
        }
        else
        {
            LOG_INFO(
                "plan subset drift: params %u != plan %u, rebuilding with the params view",
                uint32_t(s_params.subset),
                uint32_t(persisted_subset));
        }
        if (out_drifted)
            *out_drifted = true;
    }

    vfs_manifest_params params;
    params.product_file = s_params.product_file;
    params.product_url = s_params.product_url;
    params.blobs_dir = s_params.blobs_dir;
    if (!s_params.offline && vfs_fetch_started())
    {
        // manifest recursion over HTTP (downloads land in the store); the
        // cached manifests are dropped first so changed content is re-read
        vfs_purge_cached_manifests();
        params.fetch = vfs_fetch_manifest_fn;
        params.fetch_context = nullptr;
    }
    VfsPlan rebuilt;
    if (!vfs_manifest_build(params, rebuilt))
    {
        if (persisted.files.empty())
            return false;
        // a failed refresh keeps the existing cache usable - fall back
        // to the persisted plan instead of failing the boot
        LOG_WARN("update: plan refresh failed, keeping the persisted plan");
        // the fallback view honors the params' subset too (idempotent)
        vfs_manifest_apply_subset(persisted, s_params.subset);
        s_plan = persisted;
        s_plan_loaded = true;
        vfs_fetch_set_repos(persisted.manifest_repo.c_str(), persisted.file_repo.c_str());
        out_plan = persisted;
        if (out_drifted)
            *out_drifted = false; // nothing was diffed or applied
        return true;
    }
    if (!persisted.files.empty())
    {
        // only what actually changed re-fetches; a re-signed manifest
        // with identical content logs "no differences" and keeps the install
        if (out_diff)
            vfs_plan_diff(persisted, rebuilt, out_diff);
    }
    // the subset re-applies on every rebuild (drift rebuilds included)
    vfs_manifest_apply_subset(rebuilt, s_params.subset);
    // whole-file range assets that are UOP archives become per-block range
    // packs (HTTP Range streaming); offline rebuilds keep whole files
    if (!s_params.offline)
    {
        vfs_manifest_refine_ranges(rebuilt, s_params.blobs_dir, rebuilt.file_repo, 512 * 1024);
    }
    if (!vfs_plan_save(plan_path, rebuilt))
        return false;
    s_plan = rebuilt;
    s_plan_loaded = true;
    vfs_fetch_set_repos(rebuilt.manifest_repo.c_str(), rebuilt.file_repo.c_str());
    out_plan = rebuilt;
    return true;
}

// applies a plan diff to the freshly loaded state DB: changed assets
// reset to pending (journal 'R'), their pack block is invalidated (the hole
// re-opens, the file loses its sealed status) and their cached blob is
// dropped so the tiers re-fetch it. New-plan assets are pending already;
// removed ones simply drop out of plan accounting (bytes stay, harmless).
static void vfs_boot_apply_diff(const VfsPlan &plan, const vfs_plan_diff_result &diff)
{
    if (diff.changed.empty() && diff.added.empty() && diff.removed == 0)
    {
        LOG_INFO("update: new serial, no differences");
        return;
    }
    LOG_INFO(
        "update: %zu changed, %zu new, %zu removed assets",
        diff.changed.size(),
        diff.added.size(),
        diff.removed);

    vfs_store_params store_params;
    store_params.blobs_dir = s_params.blobs_dir;
    store_params.install_dir = s_params.install_dir;
    store_params.state_dir = s_params.state_dir;
    for (const auto &item : diff.changed)
    {
        if (item.file_index >= plan.files.size())
            continue;
        const auto &file = plan.files[item.file_index];
        if (item.asset_index >= file.assets.size())
            continue;
        const auto &asset = file.assets[item.asset_index];
        const size_t gi = vfs_state_base_index(plan, item.file_index) + item.asset_index;
        if (gi < s_states.size())
            s_states[gi] = vfs_pending;
        vfs_journal_mark(s_journal, { 'R', item.file_index, item.asset_index });
        if (s_arena != nullptr)
        {
            // arena mode: there is no pack block to re-open - drop the
            // granule so the next ensure re-materializes the new content
            s_arena->invalidate(asset);
        }
        else
        {
            vfs_store_invalidate_asset(plan, store_params, item.file_index, item.asset_index);
        }
        // the blob tail is content-addressed: a changed asset re-downloads it
        fs_del(fs_path_join(s_params.blobs_dir, asset.blob));
    }
}

// fold point: flush progress into state.bin, rotate the journal, reopen it
// for runtime marks. The fold is the session's durability barrier; runtime
// marks flush only (process kills keep flushed data, and the next boot's
// reconcile promotes on-disk truth). The old T1 fold left the journal CLOSED
// for the rest of the session: every later mark failed (12k+ WARN lines per
// boot) and killed sessions lost all post-fold progress from state.bin.
static void vfs_state_fold(const VfsPlan &plan)
{
    std::unique_lock<std::mutex> lock(s_state_mutex);
    vfs_state_close(plan, s_states, s_journal);
    if (vfs_journal_open(s_journal, fs_path_join(s_params.state_dir, "journal.log")))
        s_journal.durable = false; // runtime marks: flush, no fsync
}

// byte-budgeted chunk of `items` starting at `pos` (count-capped by
// VFS_BOOT_T1_CHUNK): bounds how long one materialize call holds the state
// lock, so the game's mmaps and traps never queue behind a 100+ MB write
static size_t vfs_boot_chunk_span(const std::vector<vfs_boot_item> &items, size_t pos)
{
    const size_t count = std::min(size_t(VFS_BOOT_T1_CHUNK), items.size() - pos);
    uint64_t bytes = 0;
    size_t take = 0;
    while (take < count)
    {
        bytes += items[pos + take].expected;
        ++take;
        if (bytes >= VFS_BOOT_T1_CHUNK_BYTES)
            break;
    }
    return take;
}

// post-frame convergence: the "cache rule" (everything already in the store
// materializes without the network) run in byte-budgeted batches with pauses
// between them. It used to be a single full-plan materialize right after T1,
// which put ~0.9 GB of writes (and the state lock) in front of the first
// frame whenever a previous session had left blobs in the store.
static void vfs_converge_from_store(const VfsPlan &plan)
{
    LOG_INFO("convergence: materializing store content (post-frame)");
    std::vector<vfs_boot_item> batch;
    batch.reserve(128);
    uint64_t batch_bytes = 0;
    uint64_t folded_bytes = 0;
    vfs_materialize_stats total;
    auto last_fold = std::chrono::steady_clock::now();
#if defined(XUO_EMSC)
    // the web client sits on the splash while this runs: report real progress
    // (it used to be silent for the whole write pass, so the splash looked
    // frozen and the ~0.95 progress the caller published was reset)
    size_t conv_total_assets = 0;
    size_t conv_done_assets = 0;
    for (const auto &file : plan.files)
    {
        for (const auto &asset : file.assets)
        {
            if (!vfs_asset_is_hole(asset))
                ++conv_total_assets;
        }
    }
    const size_t conv_total = conv_total_assets;
    LOG_INFO("convergence: total assets to process: %zu", conv_total);
#endif

    const auto flush_batch = [&](bool final_batch)
    {
        if (batch.empty())
            return;
        if (!s_boot_shutdown)
        {
#if defined(XUO_EMSC)
            // web: the store does not have every blob yet (a fresh browser has
            // an empty store), so fetch the chunk's missing blobs BEFORE
            // materializing - the native post-frame convergence can rely on
            // the store because the native boot already fetched T0..T3. This
            // is what left MainMisc/string_dictionary/gumpart/fonts as zero
            // slots: the materializer only reads the store, it never fetches.
            if (!s_params.offline && vfs_fetch_started())
            {
                std::vector<astr_t> tails;
                std::vector<uint64_t> sizes;
                for (const auto &item : batch)
                {
                    const auto &asset =
                        plan.files[item.file_index].assets[item.asset_index];
                    if (vfs_asset_is_hole(asset) || asset.blob_offset != 0)
                        continue; // holes have no blob; refined blocks stream on ensure
                    tails.push_back(item.tail);
                    sizes.push_back(item.expected);
                }
                if (!tails.empty())
                    vfs_fetch_batch(tails, sizes, VFS_FETCH_PRIO_BG, false, nullptr);
            }
#endif
            vfs_materialize_stats stats;
            vfs_boot_materialize(plan, &batch, &stats, true);
            total.assets_total += stats.assets_total;
            total.assets_fetched += stats.assets_fetched;
            total.assets_present += stats.assets_present;
            total.assets_missing += stats.assets_missing;
            total.assets_failed += stats.assets_failed;
            total.bytes_written += stats.bytes_written;
#if defined(XUO_EMSC)
            conv_done_assets += batch.size();
            if (conv_total != 0 && (conv_done_assets & 1023) < batch.size())
            {
                LOG_INFO("convergence: %zu of %zu assets", conv_done_assets, conv_total);
            }
#endif
        }
        batch.clear();
        batch_bytes = 0;
        if (!final_batch && !s_boot_shutdown)
            std::this_thread::sleep_for(std::chrono::milliseconds(VFS_CONV_BATCH_PAUSE_MS));
    };

    size_t gi = 0;
    for (size_t fi = 0; fi < plan.files.size() && !s_boot_shutdown; ++fi)
    {
        const auto &file = plan.files[fi];
        for (size_t ai = 0; ai < file.assets.size(); ++ai, ++gi)
        {
            const auto &asset = file.assets[ai];
            if (vfs_asset_is_hole(asset))
                continue;
            vfs_boot_item item;
            item.file_index = fi;
            item.asset_index = ai;
            item.global_index = gi;
            item.tail = asset.blob;
            item.expected = asset.ct == 0 ? uint64_t(asset.ul) : uint64_t(asset.cl);
            item.name = vfs_asset_is_loose(asset) ? asset.name : file.name;
            batch.push_back(item);
            batch_bytes += item.expected;
            if (batch_bytes < VFS_CONV_BATCH_BYTES)
                continue;
            flush_batch(false);
            folded_bytes += VFS_CONV_BATCH_BYTES;
            const auto now = std::chrono::steady_clock::now();
            if (folded_bytes >= VFS_CONV_FOLD_BYTES ||
                std::chrono::duration<double>(now - last_fold).count() >= VFS_CONV_FOLD_SECS)
            {
                vfs_state_fold(plan);
                folded_bytes = 0;
                last_fold = std::chrono::steady_clock::now();
            }
        }
    }
    flush_batch(true);
    LOG_INFO(
        "convergence: %zu assets (%zu written, %zu present, %zu missing, %zu failed, %llu bytes)",
        total.assets_total,
        total.assets_fetched,
        total.assets_present,
        total.assets_missing,
        total.assets_failed,
        (unsigned long long)total.bytes_written);
}
// processes one byte-budgeted chunk of `items` from `pos`: range assets
// stream their own block on this thread (at `priority`), whole blobs go
// through one batched fetch, then the chunk materializes (arena: residency).
// The sweep re-checks the state db per chunk: the game may have demand-
// fetched (or stubbed) an item while it sat queued behind T1
static void vfs_boot_run_chunk(
    const VfsPlan &plan,
    const vfs_boot_item *items,
    size_t count,
    int priority,
    bool recheck)
{
    std::vector<vfs_boot_item> batch_items; // whole-blob fetch + materialize
    batch_items.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        const vfs_boot_item &item = items[i];
        if (recheck)
        {
            std::unique_lock<std::mutex> lock(s_state_mutex);
            if (
                item.global_index < s_states.size() &&
                (s_states[item.global_index] == vfs_fetched ||
                 s_states[item.global_index] == vfs_stubbed))
                continue; // the game healed it first: nothing to do here
        }
        const auto &asset = plan.files[item.file_index].assets[item.asset_index];
        if (s_arena == nullptr && asset.blob_offset != 0 && !s_params.offline)
        {
            // range assets stream their own block; routing them through
            // the whole-blob batch would pull the entire source file
            // (up to 264MB) for a 10KB block
            vfs_fetch_and_materialize_asset(
                uint32_t(item.file_index), uint32_t(item.asset_index), 0, false, priority);
            continue;
        }
        batch_items.push_back(item);
    }
    if (!s_params.offline && !batch_items.empty())
    {
        std::vector<astr_t> tails;
        std::vector<uint64_t> sizes;
        for (const auto &item : batch_items)
        {
            const auto &asset = plan.files[item.file_index].assets[item.asset_index];
            if (asset.blob_offset != 0)
                continue; // refined block: the source is never a stored blob,
                          // the ensure below streams the block by range
            tails.push_back(item.tail);
            sizes.push_back(item.expected);
        }
        vfs_fetch_batch(tails, sizes, priority, false, nullptr);
    }
    if (s_boot_shutdown)
        return;
    if (s_arena != nullptr)
        vfs_boot_arena_ensure(plan, &batch_items, s_arena);
    else
        // quiet: chunk-sized stats lines are noise (one per downloaded
        // asset); the tier/convergence aggregates carry the numbers
        vfs_boot_materialize(plan, &batch_items, nullptr, true);
}

// the background thread: T1 fetch (ordered, byte-budgeted chunks) and the
// update wave, then - once the game rendered its first frame - the
// store -> install convergence, and finally the continuous sweep: every
// remaining on-demand asset (T2 then T3) downloads strictly below demand
// priority until the install is complete. Runs until done or shutdown;
// failed blobs are logged and stay missing (retried on the next boot)
#if defined(XUO_EMSC)
// web boot: the client polls this while the splash is up (wasm has no
// fault-driven paging, so the game init holds until the streamed install is
// complete on disk - the sweep keeps running in the background after this)

bool vfs_boot_web_poll()
{
    return s_boot_web_done.load();
}

float vfs_boot_web_progress()
{
    // live ratio over the transport byte accounting (http.cpp): the splash
    // keeps moving through the whole session - the streaming lanes continue
    // fetching after the boot tiers hand over, and the ratio follows. The
    // denominator grows as the plan and the lanes learn sizes; unregistered
    // traffic clamps through max(total, done) and the 0.99 cap below (1.0
    // belongs to the poll() flip - the splash pins it and stops polling)
    if (s_boot_web_done.load())
        return 1.0f;
    const uint64_t done = vfs_web_bytes_done();
    const uint64_t total = vfs_web_bytes_total();
    if (done == 0 || total == 0)
        return 0.0f; // nothing fetched or nothing known yet: keep the bar down
    float f = float(double(done) / double(total > done ? total : done));
    return f > 0.99f ? 0.99f : f;
}
#endif

#if defined(XUO_EMSC)
static void vfs_boot_verify_pack(const VfsPlan &plan, size_t file_index);

// install-completion marker: written once the whole first-session install is
// materialized; a later boot with this marker skips the streaming tiers
// entirely - the game reads the materialized files, granule work would be
// pure waste (the arena is volatile, files are what persist)
static fs_path vfs_boot_web_marker_file()
{
    return fs_path_join(fs_path_join(s_params.install_dir, "cache"), "webinstall.marker");
}

static bool vfs_boot_web_install_done(const VfsPlan &plan)
{
    std::vector<uint8_t> data;
    const bool read_ok = fs_file_read(fs_path_ascii(vfs_boot_web_marker_file()), data);
    uint64_t serial = 0;
    const bool serial_ok = vfs_product_serial(s_params.product_file, &serial);
    const astr_t expect =
        "webinstall " + std::to_string(serial) + " " + std::to_string(plan.files.size()) + "\n";
    const bool ok = read_ok && serial_ok && data.size() == expect.size() &&
                    memcmp(data.data(), expect.data(), expect.size()) == 0;
    LOG_INFO(
        "web install marker: read %d (%zu bytes), serial %d (%llu), match %d",
        int(read_ok),
        data.size(),
        int(serial_ok),
        (unsigned long long)serial,
        int(ok));
    return ok;
}

static void vfs_boot_web_install_marker_write(const VfsPlan &plan)
{
    uint64_t serial = 0;
    if (!vfs_product_serial(s_params.product_file, &serial))
        return;
    const astr_t text =
        "webinstall " + std::to_string(serial) + " " + std::to_string(plan.files.size()) + "\n";
    if (!fs_file_write(fs_path_ascii(vfs_boot_web_marker_file()), text))
        LOG_WARN("could not write the web install marker");
}

// the web persistence driver: the C side owns every byte moved between MEMFS
// (its stdio proxies to the shared browser-thread filesystem) and the OPFS
// handles JS opened. The set covers what the game reads as files at init plus
// the state db and the install marker - blobs are NOT persisted (they are
// re-fetchable granule sources; persisting ~500MB of them doubles storage).

// files the game's file manager opens as files and reads through baked
// pointers at init (LoadWithUop/LoadCommon): the arena keeps their bytes as
// granules only, so they must be assembled on disk before the game proceeds.
// Deliberately absent: soundLegacyMUL.uop (168MB - its blocks read through
// the arena on demand, see vfs_asset_read + the planned sound index), the
// map uops (the map muls are the fallback), AnimationFrame* (resolve-only,
// phase 2) and anim.mul (the excluded 190MB legacy animation set - LoadCommon
// tolerates its absence on a >=7.0 client).
static const char *const VFS_WEB_PACK_MATERIALIZE[] = {
    "MainMisc.uop",
    "string_dictionary.uop",
    "artLegacyMUL.uop",
    "gumpartLegacyMUL.uop",
    "fonts.mul",
    "multi.idx",
    "multi.mul",
    "animdata.mul",
    "tiledata.mul",
    "hues.mul",
    "radarcol.mul",
    "skills.idx",
    "skills.mul",
    "speech.mul",
    "langcode.iff",
    "multimap.rle",
    "texidx.mul",
    "texmaps.mul",
    "lightidx.mul",
    "light.mul",
    "artidx.mul",
    "gumpidx.mul",
    "soundidx.mul",
    "anim.idx",
    "unifont.mul",
    "unifont1.mul",
    "unifont2.mul",
    "unifont3.mul",
    "unifont4.mul",
    "unifont5.mul",
    "unifont6.mul",
    "unifont7.mul",
    "unifont8.mul",
    "unifont9.mul",
    "unifont10.mul",
    "unifont11.mul",
    "unifont12.mul",
    "unifont13.mul",
    "unifont14.mul",
    "unifont15.mul",
    "unifont16.mul",
    "unifont17.mul",
    "unifont18.mul",
    "unifont19.mul",
};

static std::vector<astr_t> s_webPersistPaths;


static void vfs_boot_web_mkdirs(const char *path);





static void vfs_boot_web_mkdirs(const char *path)
{
    // the caller names the root (the per-shard streaming install dir)
    const auto root = fs_path_from(path);
    fs_path_create(root);
    fs_path_create(fs_path_join(root, "cache"));
}







static void vfs_boot_materialize_packs(const VfsPlan &plan)
{
    for (const char *name : VFS_WEB_PACK_MATERIALIZE)
    {
        bool found = false;
        for (size_t fi = 0; fi < plan.files.size() && !found; ++fi)
        {
            if (strcasecmp(plan.files[fi].name.c_str(), name) != 0)
                continue;
            found = true;
            const auto started = std::chrono::steady_clock::now();
            // arena-free: the payloads come from the blob store, one asset at
            // a time - a full-plan arena residency cannot fit the wasm heap
            // next to the install files (the old path is what failed ensures
            // for the late/low-priority packs, e.g. fonts and string_dictionary)
            const vfs_pack_write_stats stats = vfs_store_materialize_pack_from_store(
                plan, fi, s_params.install_dir, s_params.blobs_dir);
            const float secs =
                std::chrono::duration<float>(std::chrono::steady_clock::now() - started).count();
            const auto &file = plan.files[fi];
            const bool sparse = !file.is_uop;
            if (stats.stubbed == 0 && stats.failed == 0)
            {
                LOG_INFO(
                    "web pack ready: %s (%zu written%s, %.1fs)",
                    name,
                    stats.written,
                    stats.skipped != 0 ? ", already present" : "",
                    secs);
            }
            else
            {
                // the census is the point: the web boot proceeds with stub
                // slots (the game reads zeros there), so the boot log must
                // name exactly which packs are incomplete and by how much
                LOG_ERROR(
                    "web pack INCOMPLETE: %s (written %zu, stubbed %zu, failed %zu, %.1fs) - "
                    "stub slots read as zeros in game",
                    name,
                    stats.written,
                    stats.stubbed,
                    stats.failed,
                    secs);
            }
            s_boot_web_stub_assets += stats.stubbed + stats.failed;
            if (sparse && stats.stubbed != 0)
                LOG_WARN("web pack %s is a loose whole-file asset: a missing blob leaves it empty", name);
        }
        if (!found)
            LOG_WARN("web pack materialize: %s not in the plan", name);
    }
    LOG_INFO(
        "web packs ready: heap in use %.1f MB, %zu stub assets in materialized packs",
        double(reinterpret_cast<uintptr_t>(sbrk(0))) / 1048576.0,
        s_boot_web_stub_assets);
    for (const char *name : VFS_WEB_PACK_MATERIALIZE)
    {
        for (size_t fi = 0; fi < plan.files.size(); ++fi)
        {
            if (strcasecmp(plan.files[fi].name.c_str(), name) == 0)
            {
                vfs_boot_verify_pack(plan, fi);
                break;
            }
        }
    }
}

// post-materialization integrity probe for one pack: re-opens the file the
// way the game does (mmap) and, for a few sample assets, checks
//   (a) the write path - mapped bytes == arena granule bytes
//   (b) the range path - a fresh CACHE-BUSTED re-fetch inflates to the same
//       granule bytes (a mismatch means an earlier fetch served stale bytes,
//       e.g. a poisoned browser 206 cache; the query string is ignored by
//       the proxy and by the file repo)
static void vfs_boot_verify_pack(const VfsPlan &plan, size_t file_index)
{
    const auto &file = plan.files[file_index];
    uint8_t *start = nullptr;
    size_t mapped = 0;
    if (!vfs_map_file(fs_path_join(s_params.install_dir, file.name), start, mapped) ||
        mapped != file.size)
    {
        LOG_ERROR(
            "pack verify: %s could not be mapped (%zu vs %llu)",
            file.name.c_str(),
            mapped,
            (unsigned long long)file.size);
        return;
    }
    const auto *header = (const UopHeader *)start;
    if (!file.is_uop)
    {
        vfs_unmap_file(start, mapped);
        return; // loose files (.mul, .idx) are not UOP archives
    }
    if (header->Magic != MYP_MAGIC || header->Version < 4 || header->Version > 5 ||
        header->SectionOffset != sizeof(UopHeader) ||
        header->FileCount != uint32_t(file.assets.size()))
    {
        LOG_ERROR(
            "pack verify: %s header mismatch (magic %08x ver %u off %llu count %u)",
            file.name.c_str(),
            header->Magic,
            header->Version,
            (unsigned long long)header->SectionOffset,
            header->FileCount);
        vfs_unmap_file(start, mapped);
        return;
    }

    auto *arena = static_cast<vfs_store_arena *>(s_arena);
    int checked = 0;
    int write_bad = 0;
    int range_bad = 0;
    for (size_t ai = 0; ai < file.assets.size() && checked < 8; ai += 1 + ai % 7)
    {
        const auto &asset = file.assets[ai];
        if (vfs_asset_is_hole(asset) || asset.ul == 0)
            continue;
        ++checked;
        std::vector<uint8_t> granule(size_t(asset.ul), 0);
        const bool resident = arena != nullptr && arena->ensure(asset, true) &&
                              arena->read(asset, 0, granule.data(), granule.size());
        // (a) the write path: mapped bytes vs the granule
        if (resident &&
            memcmp(start + asset.file_offset + asset.ml, granule.data(), granule.size()) != 0)
        {
            ++write_bad;
            LOG_ERROR("pack verify: %s asset %zu write mismatch", file.name.c_str(), ai);
        }
        // (b) the range path, independent of any HTTP cache
        if (asset.blob_offset != 0 && !s_params.offline)
        {
            const uint64_t len = vfs_asset_range_len(asset);
            const astr_t url =
                plan.file_repo + asset.blob + "?xuo=" + std::to_string(asset.blob_offset);
            std::vector<uint8_t> block(size_t(len), 0);
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
                &block,
                0,
                false);
            std::vector<uint8_t> payload(size_t(asset.ul), 0);
            bool inflated = false;
            if (res.ok && res.bytes == len && block.size() >= asset.ml)
            {
                const uint8_t *body = block.data() + asset.ml;
                const size_t body_len = block.size() - asset.ml;
                if (asset.ct == 1)
                {
                    zlib_uLongf dst_len = payload.size();
                    inflated = uncompress(
                                   payload.data(),
                                   &dst_len,
                                   reinterpret_cast<zlib_Bytef const *>(body),
                                   zlib_uLong(body_len)) == Z_OK;
                    payload.resize(size_t(dst_len));
                }
                else
                {
                    payload.assign(body, body + body_len);
                    inflated = true;
                }
            }
            if (!resident || !inflated || granule != payload)
            {
                ++range_bad;
                LOG_ERROR(
                    "pack verify: %s asset %zu range mismatch (resident %d inflated %d)",
                    file.name.c_str(),
                    ai,
                    int(resident),
                    int(inflated));
            }
        }
    }
    vfs_unmap_file(start, mapped);
    LOG_INFO(
        "pack verify: %s checked %d (write mismatches %d, range mismatches %d)",
        file.name.c_str(),
        checked,
        write_bad,
        range_bad);
}
#endif

static void vfs_boot_background(VfsPlan plan, std::vector<vfs_boot_item> items, std::vector<vfs_boot_item> sweep)
{
#if defined(XUO_EMSC)
    // warm web boot: a previous session completed this exact install and the
    // browser restored it - granule streaming would be pure waste (the game
    // reads the materialized files). Assemble anything missing and open the
    // gates; the sweep has nothing to add either.
    if (vfs_boot_web_install_done(plan))
    {
        LOG_INFO("web install marker: previous install is complete, skipping the tiers");
        vfs_boot_materialize_packs(plan);
        vfs_state_fold(plan);
        s_boot_web_done.store(true, std::memory_order_release);
        return;
    }
#endif
#if defined(XUO_EMSC)
    EM_ASM({ console.warn('ctx[boot-background] worker=' + (typeof WorkerGlobalScope !== 'undefined' && self instanceof WorkerGlobalScope)); });
    {
        static char probeErr[256] = {0};
        const astr_t install = fs_path_ascii(s_params.install_dir);
        EM_ASM(
            {
                var out = function (p)
                {
                    try
                    {
                        return p + '=' + FS.readdir(p).length + ' entries';
                    }
                    catch (e)
                    {
                        return p + ' ERR' + (e && e.errno !== undefined ? e.errno : '');
                    }
                };
                var cwd = '/';
                try { cwd = FS.cwd(); } catch (e) { cwd = 'ERR'; }
                var inst = UTF8ToString($2);
                stringToUTF8('cwd=' + cwd + ' | ' + out('/crossuo') + ' | ' + out(inst) +
                             ' | ' + out(inst + '/cache'), $0, $1);
            },
            (intptr_t)probeErr,
            sizeof(probeErr) - 1,
            (intptr_t)install.c_str());
        LOG_INFO("boot worker FS: %s", probeErr);
    }
#endif
    LOG_INFO("background tier: %zu assets", items.size());
    for (size_t pos = 0; pos < items.size() && !s_boot_shutdown;)
    {
        const size_t count = vfs_boot_chunk_span(items, pos);
        vfs_boot_run_chunk(plan, items.data() + pos, count, VFS_FETCH_PRIO_BG, false);
        pos += count;
    }
    LOG_INFO("background tier complete");
    vfs_state_fold(plan); // persist T1 (and reopen the journal)

#if defined(XUO_EMSC)
    // the web client sits on the splash poll until the install is resident
    // (s_boot_web_done below) - no game frame can arrive before convergence,
    // so the gate is cleared here: boot work is not competing with game
    // frames while only the splash renders. The game's real first frame
    // still notifies later (vfs_notify_first_frame demotes the pool).
    s_frame_seen = true;
#endif
    // convergence waits for the first frame: boot work must not compete with
    // it (headless callers get it via vfs_boot_wait, which flags the frame
    // immediately)
#if defined(XUO_EMSC)
    // yield to the worker event loop: the main worker needs to render frames
    // and the async OPFS operations need their callbacks processed
    while (!s_frame_seen && !s_boot_shutdown)
        emscripten_thread_sleep(VFS_CONV_WAIT_POLL_MS);
#else
    while (!s_frame_seen && !s_boot_shutdown)
        std::this_thread::sleep_for(std::chrono::milliseconds(VFS_CONV_WAIT_POLL_MS));
#endif
    if (!s_boot_shutdown)
    {
        if (s_arena != nullptr)
            vfs_boot_arena_ensure(plan, nullptr, s_arena); // store -> residency
        else
            vfs_converge_from_store(plan);
    }
#if defined(XUO_EMSC)
    // Web: the arena branch above must NOT run - a full-plan residency holds
    // every decompressed payload in the wasm heap (the 1GB paging heap,
    // already ~730MB deep from the streaming install), so the late ensures
    // fail on allocation and their packs materialize as zero slots (that is
    // how fonts/cliloc vanished). The store convergence instead pulls every
    // missing blob into the on-disk store at a bounded cost, and the pack
    // materialization below reads payloads from there one asset at a time.
    // The install therefore holds exactly one asset's payload in memory per
    // write, and the heap stays flat for the whole boot.
#endif
#if defined(XUO_EMSC)
    // the packs the game opens as files must exist before init proceeds
    vfs_boot_materialize_packs(plan);
#endif
#if defined(XUO_EMSC)
    // the full install is on disk: the game init can proceed (the sweep
    // continues below for as long as the session lasts)
    vfs_boot_web_install_marker_write(plan);
    s_boot_web_done.store(true, std::memory_order_release);
#endif
    // the continuous sweep: the rest of the plan converges in the background
    // for as long as the session lasts (state re-checked per chunk; the lane
    // cap keeps every demand request ahead of it)
    if (!sweep.empty() && !s_boot_shutdown)
    {
        LOG_INFO("background sweep: %zu assets", sweep.size());
        for (size_t pos = 0; pos < sweep.size() && !s_boot_shutdown;)
        {
            const size_t count = vfs_boot_chunk_span(sweep, pos);
            vfs_boot_run_chunk(plan, sweep.data() + pos, count, VFS_FETCH_PRIO_SWEEP, true);
            pos += count;
        }
        LOG_INFO("background sweep complete");
    }
    // final fold: everything the tier (and traps) did lands in state.bin
    vfs_state_fold(plan);
}

bool vfs_boot(void (*progress)(float done01, const char *label))
{
    if (!vfs_enabled())
    {
        LOG_ERROR("vfs_boot called while streaming is disabled");
        return false;
    }
    if (s_boot_started)
        return true;

    // curl globals: normally initialized by the caller (game/vuoi); guard
    // anyway - worker threads must never see uninitialized libcurl
    http_init();

    if (!vfs_params_dirs_ready())
        return false;
    if (fs_path_some(s_params.record_path))
    {
        vfs_record_configure(s_params.record_path, s_params.record_scenario.c_str());
        LOG_INFO("recording asset accesses: %s", fs_path_ascii(s_params.record_path));
    }

    const bool online = !s_params.offline;
    if (online)
    {
        vfs_fetch_params fetch_params;
        fetch_params.workers = s_params.workers;
        vfs_fetch_set_store(s_params.blobs_dir);
        if (!vfs_fetch_start(fetch_params))
            return false;
    }
    if (progress)
        progress(0.0f, "manifests");

    VfsPlan plan;
    vfs_plan_diff_result diff;
    bool drifted = false;
    if (!vfs_boot_plan(plan, &diff, &drifted))
    {
        vfs_fetch_stop();
        return false;
    }
    vfs_manifest_assign_tiers(plan);

    // granule store: the params-selected backend, created BEFORE the
    // diff application below so a diff's invalidation reaches the live
    // backend in both modes (the arena branch was unreachable when this ran
    // after the state block). The native sparse default keeps the standard
    // disk pipeline unchanged (the trap resolves through
    // vfs_store_rematerialize_asset, not through this object); arena mode
    // materializes granules into memory instead of files (heap arenas hold
    // decompressed payload bytes, no install files appear)
    if (s_params.store_backend == vfs_store_backend_arena)
    {
        vfs_store_arena_params arena_params;
        arena_params.store.blobs_dir = s_params.blobs_dir;
        std::unique_lock<std::mutex> lock(s_state_mutex);
        s_arena = vfs_store_backend_arena_create(arena_params);
        // s_plan (not the local): the backend keeps the plan POINTER - the
        // boot function's copy dies when it returns after spawning the
        // background worker, s_plan lives for the process
        if (s_arena == nullptr || !s_arena->open(s_plan))
        {
            LOG_ERROR("vfs: arena store failed to open");
            delete s_arena;
            s_arena = nullptr;
            vfs_fetch_stop();
            return false;
        }
    }

    // state db: load + journal + reconcile, then the boot skeleton
    const size_t asset_count = vfs_state_asset_count(plan);
    {
        std::unique_lock<std::mutex> lock(s_state_mutex);
        s_states.clear();
        bool state_reset = false;
        vfs_state_load(
            fs_path_join(s_params.state_dir, "state.bin"), asset_count, s_states, &state_reset);
        if (state_reset)
        {
            // journal hygiene: the DB was cleared (asset-count mismatch or
            // corruption) and the stale log must not resurrect fetched marks
            // over the fresh all-pending states. Same safety rule as the
            // rotation - flush the cleared state BEFORE the truncate, which
            // is exactly what the fold does
            vfs_state_close(plan, s_states, s_journal);
        }
        std::vector<vfs_journal_record> records;
        vfs_journal_replay(fs_path_join(s_params.state_dir, "journal.log"), records);
        vfs_state_apply_journal(plan, records, s_states);
        // the journal opens before the diff application: the update resets
        // ('R') must be journalled durably, not lost with a closed handle
        if (!vfs_journal_open(s_journal, fs_path_join(s_params.state_dir, "journal.log")))
        {
            vfs_fetch_stop();
            return false;
        }
        // update flow: reset exactly what changed (state + pack hole
        // + cached blob) before reconcile sees the fresh plan. Gated on the
        // diff itself: it is computed whenever a persisted plan exists
        // (drifted or not), and a real diff must apply either way. `drifted`
        // stays as log context only.
        const bool diff_nonempty =
            !diff.changed.empty() || !diff.added.empty() || diff.removed != 0;
        if (diff_nonempty)
            vfs_boot_apply_diff(plan, diff);
        else if (drifted)
            LOG_INFO("update: drifted plan, no differences");
        // the on-disk files are the source of truth for the sparse pipeline;
        // arena mode keeps no files (its state is granule residency) -
        // residency is the boot's own
        // record, so there is nothing to reconcile against
        if (s_arena == nullptr)
        {
            vfs_reconcile_stats reconcile;
            vfs_state_reconcile(
                plan, s_params.install_dir, s_states, &reconcile, s_params.blobs_dir);
            if (reconcile.assets_repaired)
            {
                LOG_INFO(
                    "reconcile: %zu of %zu assets downgraded to pending",
                    reconcile.assets_repaired,
                    reconcile.assets_checked);
            }
        }
    }

    vfs_store_params store_params;
    store_params.blobs_dir = s_params.blobs_dir;
    store_params.install_dir = s_params.install_dir;
    store_params.state_dir = s_params.state_dir;
    if (s_arena == nullptr && !vfs_store_prepare(plan, store_params))
    {
        LOG_ERROR("vfs: could not prepare the install skeleton");
        vfs_fetch_stop();
        return false;
    }
    fs_path_create(s_params.install_dir);

    // managed-mapping registry: file -> plan index, consumed by vfs_map_file.
    // Arena mode maps nothing (no files exist): reads go through the store's
    // read() once the game layer is wired (web port)
    if (s_arena == nullptr)
        vfs_managed_build();

    // tier work lists. Arena mode collects T0 with its fetched entries: the
    // warm-boot residency contract re-ensures the core granules at every
    // start (T1+ repopulate on access instead)
    std::vector<vfs_boot_item> t0, t0_fetch, t1, t1_fetch;
    size_t holes = 0, t2 = 0, t3 = 0;
    vfs_boot_collect(plan, vfs_tier_t0, &t0, &t0_fetch, &holes, s_arena != nullptr);
    vfs_boot_collect(plan, vfs_tier_t1, &t1, &t1_fetch, nullptr);
    {
        size_t gi = 0;
        for (const auto &file : plan.files)
        {
            for (const auto &asset : file.assets)
            {
                if (!vfs_asset_is_hole(asset) && gi < s_states.size() &&
                    s_states[gi] != vfs_fetched)
                {
                    if (asset.tier == vfs_tier_t2)
                        ++t2;
                    else if (asset.tier == vfs_tier_t3)
                        ++t3;
                }
                ++gi;
            }
        }
    }
    LOG_INFO(
        "boot: %zu T0 (%zu to fetch), %zu T1 (%zu to fetch), T2 %zu, T3 %zu, holes %zu",
        t0.size(),
        t0_fetch.size(),
        t1.size(),
        t1_fetch.size(),
        t2,
        t3,
        holes);
    vfs_boot_order_t1(plan, &t1);
    vfs_boot_order_t1(plan, &t1_fetch);
    // the update wave rides at the END of the background tier: real T1
    // traffic first, then what the plan diff changed in the on-demand tiers
    for (const auto &item : diff.changed)
        vfs_boot_wave_append(plan, item, &t1);
    for (const auto &item : diff.added)
        vfs_boot_wave_append(plan, item, &t1);

    // the continuous sweep list: every remaining on-demand asset, T2 before
    // T3 (plan order within a tier). Collected here (boot thread, states
    // still exclusive) and re-checked per chunk at run time - the game may
    // demand-heal items while the sweep sits queued behind T1. Stubbed
    // assets keep their on-demand heal path; the sweep never touches them
    std::vector<vfs_boot_item> sweep;
    if (s_params.background_sweep)
    {
        vfs_boot_collect(plan, vfs_tier_t2, &sweep, nullptr, nullptr);
        vfs_boot_collect(plan, vfs_tier_t3, &sweep, nullptr, nullptr);
        sweep.erase(
            std::remove_if(
                sweep.begin(),
                sweep.end(),
                [](const vfs_boot_item &item)
                {
                    return item.global_index >= s_states.size() ||
                           s_states[item.global_index] == vfs_fetched ||
                           s_states[item.global_index] == vfs_stubbed;
                }),
            sweep.end());
    }

#if defined(XUO_EMSC)
    // -----------------------------------------------------------------
    // web boot: punctual mode.
    // The pack files become table-only skeletons (a few hundred KB each);
    // every block read is intercepted at use time by the punctual fetch
    // layer (vfs_asset_fetch_block_*), with the browser HTTP cache as the
    // persistent store. Loose files of the plan (fonts, tiledata, hues,
    // ...) are small (< 10 MB) and materialize whole as real files. There
    // is no T1 residency, no convergence, no sweep: a full-plan residency
    // cannot fit the wasm heap next to the game (it OOM'd the browser at
    // ~3 GB), and the lazy path needs none of it.
    // -----------------------------------------------------------------
    vfs_materialize_stats web_stats;
    std::vector<vfs_boot_item> loose;
    {
        size_t gi = 0;
        for (size_t fi = 0; fi < plan.files.size(); ++fi)
        {
            const auto &file = plan.files[fi];
            if (file.streamable)
            {
                // subset-retained metadata: the boot install never touches
                // these bytes (the stream-file lane acquires them on demand)
                gi += file.assets.size();
                continue;
            }
            for (size_t ai = 0; ai < file.assets.size(); ++ai, ++gi)
            {
                if (file.is_uop || vfs_asset_is_hole(file.assets[ai]))
                    continue;
                const auto &asset = file.assets[ai];
                vfs_boot_item item;
                item.file_index = fi;
                item.asset_index = ai;
                item.global_index = gi;
                item.tail = asset.blob;
                item.expected = asset.ct == 0 ? uint64_t(asset.ul) : uint64_t(asset.cl);
                item.name = vfs_asset_is_loose(asset) ? asset.name : file.name;
                loose.push_back(item);
            }
        }
    }
    size_t uop_packs = 0;
    for (const auto &f : plan.files)
        uop_packs += f.is_uop ? 1 : 0;
    LOG_INFO("web boot: %zu loose files, %zu uop packs (skeleton)", loose.size(), uop_packs);

    // loose size policy: files up to 10MB materialize as real files (the
    // game mmaps them). A bounded set of larger files is exempt: the
    // statics muls (random access through staidx positions - converting
    // them to ranged fetches touches every statics consumer) and texmaps
    // (texture decode reads). The rest - legacy anim muls, facet maps,
    // Music, the x-variant map uops - is skipped: UOP animations replace
    // the anim muls, and nothing reads the others on web.
    static const char *const VFS_WEB_LOOSE_EXEMPT[] = {
        "statics0.mul", "statics0x.mul", "statics1.mul", "statics1x.mul",
        "statics2.mul", "statics3.mul", "statics4.mul", "statics5.mul",
        "texmaps.mul",
    };
    auto loose_exempt = [](const astr_t &name)
    {
        for (const char *candidate : VFS_WEB_LOOSE_EXEMPT)
        {
            if (strcasecmp(name.c_str(), candidate) == 0)
                return true;
        }
        return false;
    };

    size_t loose_skipped = 0;
    uint64_t loose_skipped_bytes = 0;
    {
        std::vector<vfs_boot_item> keep;
        keep.reserve(loose.size());
        for (const auto &item : loose)
        {
            if (item.expected > 10ull * 1024 * 1024 && !loose_exempt(item.name))
            {
                ++loose_skipped;
                loose_skipped_bytes += item.expected;
                continue;
            }
            keep.push_back(item);
        }
        loose.swap(keep);
    }
    if (loose_skipped != 0)
    {
        LOG_WARN(
            "web boot: skipping %zu large loose files (%.0f MB): legacy anims, facet maps, music",
            loose_skipped,
            double(loose_skipped_bytes) / 1048576.0);
    }

    // denominator seeding: every loose file the boot intends to fetch
    // registers its expected size before the fetch pass - absent blobs only,
    // present ones add no traffic, so the ratio stays honest across partial
    // installs (granule/stream traffic registers later, as the lanes learn it)
    uint64_t web_plan_bytes = 0;
    for (const auto &item : loose)
    {
        const auto &asset = plan.files[item.file_index].assets[item.asset_index];
        if (!fs_path_exists(fs_path_join(s_params.blobs_dir, asset.blob)))
            web_plan_bytes += item.expected;
    }
    vfs_web_bytes_expect(web_plan_bytes);
    LOG_INFO("web boot: %.1f MB planned (loose tier denominator)", double(web_plan_bytes) / 1048576.0);

    if (online && !loose.empty())
    {
        std::vector<astr_t> tails;
        std::vector<uint64_t> sizes;
        tails.reserve(loose.size());
        sizes.reserve(loose.size());
        for (const auto &item : loose)
        {
            tails.push_back(item.tail);
            sizes.push_back(item.expected);
        }
        if (!vfs_fetch_batch(tails, sizes, 0, false, progress))
        {
            LOG_WARN("vfs boot: some loose files could not be downloaded");
        }
        vfs_boot_materialize(plan, &loose, &web_stats);
        LOG_INFO(
            "web boot: loose materialized (%zu files, %.1f MB written, %zu failed)",
            web_stats.assets_fetched + web_stats.assets_present,
            double(web_stats.bytes_written) / 1048576.0,
            web_stats.assets_failed);
    }
    else
    {
        LOG_WARN("web boot: offline or no loose files - skipping the loose tier");
    }

    // skeletons for every uop pack (rewritten each boot - cheap and
    // self-repairing; a few hundred KB per pack)
    size_t skeletons = 0;
    uint64_t skeleton_bytes = 0;
    for (size_t fi = 0; fi < plan.files.size(); ++fi)
    {
        const auto &file = plan.files[fi];
        if (!file.is_uop)
            continue;
        uint64_t size = 0;
        if (vfs_store_materialize_skeleton(plan, fi, s_params.install_dir, &size))
        {
            ++skeletons;
            skeleton_bytes += size;
        }
        else
        {
            LOG_ERROR("vfs boot: skeleton failed: %s (pack loads will fail)", file.name.c_str());
        }
    }
    LOG_INFO(
        "web boot: %zu skeletons (%.1f MB), heap %.1f MB",
        skeletons,
        double(skeleton_bytes) / 1048576.0,
        double(reinterpret_cast<uintptr_t>(sbrk(0))) / 1048576.0);
    vfs_state_fold(plan);
    s_boot_shutdown = false;
    s_boot_started = true;
    s_boot_web_done.store(true, std::memory_order_release);
    if (progress)
        progress(1.0f, "ready");
    return true;
#endif
    // T0: blocking fetch + materialize with progress on this thread
    if (!t0_fetch.empty() && online)
    {
        std::vector<astr_t> tails;
        std::vector<uint64_t> sizes;
        vfs_boot_items_to_tails(t0_fetch, &tails, &sizes);
        if (!vfs_fetch_batch(tails, sizes, 0, false, progress))
        {
            LOG_WARN("vfs: some core assets could not be downloaded");
        }
    }

    vfs_materialize_stats stats;
    if (s_arena != nullptr)
        vfs_boot_arena_ensure(plan, &t0, s_arena, true); // T0 resident at every boot
    else
        vfs_boot_materialize(plan, &t0, &stats);

    // every core asset must be in place: a missing T0 file is a boot failure
    // naming the file
    bool failed = false;
    {
        std::unique_lock<std::mutex> lock(s_state_mutex);
        for (const auto &item : t0)
        {
            if (item.global_index >= s_states.size() || s_states[item.global_index] != vfs_fetched)
            {
                LOG_ERROR("vfs: core asset missing: %s", item.name.c_str());
                failed = true;
            }
        }
    }
    if (failed)
    {
        LOG_ERROR("vfs: boot failed in the blocking tier");
        vfs_fetch_stop();
        vfs_runtime_close(); // journal + recorder were opened before T0
        if (progress)
            progress(1.0f, "failed");
#if defined(XUO_EMSC)
        s_boot_web_done.store(true, std::memory_order_release);
#endif
        return false;
    }
    // fold point for the blocking tier (reopens the journal in runtime mode:
    // marks from here on flush, folds are the durability barriers)
    vfs_state_fold(plan);
    if (progress)
        progress(1.0f, "starting");

    // trap backend: bound before any managed mapping can exist; the
    // fault handler itself installs lazily with the first vfs_trap_map, so a
    // run that never maps a managed file never installs one. Arena mode has
    // no managed mappings (no traps on the arena backend) and skips it.
    if (s_arena == nullptr)
        vfs_trap_backend_ensure();

    // T1: background worker loop (never blocks the game render loop)
    s_boot_shutdown = false;
    s_boot_started = true;
    s_background = std::thread(vfs_boot_background, plan, std::move(t1), std::move(sweep));
    return true;
}

void vfs_shutdown()
{
    // the trap goes first: no resolution may run against a torn-down store
    // (later faults outside managed ranges chain to the default crash)
    vfs_trap_shutdown();
    if (s_boot_started)
    {
        // signal the background tier BEFORE stopping the pool: the flag
        // ends the tier's gates and chunk loops (the first-frame wait and
        // the sweep would otherwise walk every remaining chunk against a
        // dead pool, hanging the join below for the whole install)
        s_boot_shutdown = true;
        vfs_fetch_stop(); // unblocks any in-flight batch in the background tier
        if (s_background.joinable())
            s_background.join();
        {
            std::unique_lock<std::mutex> lock(s_state_mutex);
            vfs_state_close(s_plan, s_states, s_journal);
            s_states.clear();
        }
    }
    vfs_runtime_close(); // also covers boot runs that never reached T1
    s_boot_started = false;
    LOG_INFO("vfs: shutdown complete");
}

void vfs_boot_wait()
{
    // headless contract: waiting means "finish everything boot would do",
    // including the post-frame convergence the game defers
    s_frame_seen = true;
    if (s_background.joinable())
        s_background.join();
}

#if defined(XUO_EMSC)
// web boot wait: flags the first frame (unblocking the convergence gate in
// the background tier) and returns once the streamed install is fully on
// disk. Never joins the background thread - the sweep continues running for
// the whole session and the browser main thread must not be blocked.
void vfs_boot_web_wait()
{
    if (!s_boot_started)
        return;
    s_frame_seen = true;
    while (!s_boot_web_done.load() && !s_boot_shutdown)
#if defined(XUO_EMSC)
        emscripten_thread_sleep(100);
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
}
#endif

// the game calls this once the first frame is out: post-frame convergence
// (store -> install) may start spending disk behind the render loop, and
// the download pool demotes to in-game mode - one worker parks (the pool
// runs N-1 of its N threads; the default pool is cores-1, so the game
// keeps two cores clear) and the sweep lane caps at active-1 so every
// on-demand request stays ahead of the continuous sweep
void vfs_notify_first_frame()
{
    s_frame_seen = true;
    vfs_fetch_set_ingame();
}

// full install: queue the blob of every planned
// asset that is not in the store yet, all tiers (T0..T3), and wait. The
// thin xuoi --full driver runs this after a boot, then materializes.
// Nothing to fetch is success regardless of the network: a complete store
// works offline (zero requests), while offline + a missing blob fails.
// REFINED range assets are skipped here: their blocks stream per-block
// through the background sweep and the trap (fetching their whole source
// blob here would pull entire packs for a handful of blocks)
bool vfs_fetch_all(void (*progress)(float done01, const char *label))
{
    if (!vfs_enabled() || !vfs_plan_ready())
        return false;
    std::vector<astr_t> tails;
    std::vector<uint64_t> sizes;
    size_t total = 0;
    {
        std::unique_lock<std::mutex> lock(s_state_mutex);
        for (const auto &file : s_plan.files)
        {
            if (file.streamable)
                continue; // stream-lane bytes: never whole-blob fetched
            for (const auto &asset : file.assets)
            {
                ++total;
                if (vfs_asset_is_hole(asset))
                    continue;
                if (asset.blob_offset != 0)
                    continue; // refined range asset: the sweep streams its blocks
                if (fs_path_exists(fs_path_join(s_params.blobs_dir, asset.blob)))
                    continue; // cached: the materialize pass converges it
                tails.push_back(asset.blob);
                sizes.push_back(asset.ct == 0 ? uint64_t(asset.ul) : uint64_t(asset.cl));
            }
        }
    }
    LOG_INFO("fetch-all: %zu of %zu assets to download", tails.size(), total);
    if (tails.empty())
        return true;
    if (s_params.offline || !vfs_fetch_started())
        return false;
    return vfs_fetch_batch(tails, sizes, 1, false, progress);
}

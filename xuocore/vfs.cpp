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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#if defined(XUO_WINDOWS)
#include <windows.h> // MoveFileExA (atomic product manifest replace)
#endif

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
        if (index >= 0)
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
    if (io->Address == 0u)
        return false; // nodraw hole: genuinely no asset
    if (io->Width != 0 && io->Height != 0)
        return true; // materialized at index build or already resolved
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
                const uint8_t state = gi < s_states.size() ? s_states[gi] : vfs_pending;
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
            if (vfs_asset_is_hole(asset) ||
                fs_path_exists(fs_path_join(s_params.blobs_dir, asset.blob)))
                continue;
            tails.push_back(item.tail);
            sizes.push_back(item.expected);
        }
        vfs_fetch_batch(tails, sizes, 1, false, nullptr);
    }

    bool ok = true;
    for (const auto &item : *items)
    {
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

    const auto flush_batch = [&](bool final_batch)
    {
        if (batch.empty())
            return;
        if (!s_boot_shutdown)
        {
            vfs_materialize_stats stats;
            vfs_boot_materialize(plan, &batch, &stats, true);
            total.assets_total += stats.assets_total;
            total.assets_fetched += stats.assets_fetched;
            total.assets_present += stats.assets_present;
            total.assets_missing += stats.assets_missing;
            total.assets_failed += stats.assets_failed;
            total.bytes_written += stats.bytes_written;
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
        vfs_boot_items_to_tails(batch_items, &tails, &sizes);
        vfs_fetch_batch(tails, sizes, priority, false, nullptr);
    }
    if (s_boot_shutdown)
        return;
    if (s_arena != nullptr)
        vfs_boot_arena_ensure(plan, &batch_items, s_arena);
    else
        vfs_boot_materialize(plan, &batch_items, nullptr);
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
static std::atomic<bool> s_boot_web_done{false};
static std::atomic<float> s_boot_web_progress{0.0f};

bool vfs_boot_web_poll()
{
    return s_boot_web_done.load();
}

float vfs_boot_web_progress()
{
    return s_boot_web_progress.load();
}
#endif

static void vfs_boot_background(VfsPlan plan, std::vector<vfs_boot_item> items, std::vector<vfs_boot_item> sweep)
{
    LOG_INFO("background tier: %zu assets", items.size());
    const float background_total = items.size() > 0 ? float(items.size()) : 1.0f;
    for (size_t pos = 0; pos < items.size() && !s_boot_shutdown;)
    {
        const size_t count = vfs_boot_chunk_span(items, pos);
        vfs_boot_run_chunk(plan, items.data() + pos, count, VFS_FETCH_PRIO_BG, false);
        pos += count;
#if defined(XUO_EMSC)
        s_boot_web_progress.store(0.10f + 0.85f * (float(pos) / background_total),
                                  std::memory_order_relaxed);
#endif
    }
    LOG_INFO("background tier complete");
    vfs_state_fold(plan); // persist T1 (and reopen the journal)

    // convergence waits for the first frame: boot work must not compete with
    // it (headless callers get it via vfs_boot_wait, which flags the frame
    // immediately)
    while (!s_frame_seen && !s_boot_shutdown)
        std::this_thread::sleep_for(std::chrono::milliseconds(VFS_CONV_WAIT_POLL_MS));
    if (!s_boot_shutdown)
    {
        if (s_arena != nullptr)
            vfs_boot_arena_ensure(plan, nullptr, s_arena); // store -> residency
        else
            vfs_converge_from_store(plan);
    }
#if defined(XUO_EMSC)
    // the full install is on disk: the game init can proceed (the sweep
    // continues below for as long as the session lasts)
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
#if defined(XUO_EMSC)
    s_boot_web_progress.store(0.02f, std::memory_order_relaxed);
#endif
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
        if (s_arena == nullptr || !s_arena->open(plan))
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
        s_boot_web_progress.store(1.0f, std::memory_order_relaxed);
        s_boot_web_done.store(true, std::memory_order_release);
#endif
        return false;
    }
    // fold point for the blocking tier (reopens the journal in runtime mode:
    // marks from here on flush, folds are the durability barriers)
    vfs_state_fold(plan);
#if defined(XUO_EMSC)
    s_boot_web_progress.store(0.10f, std::memory_order_relaxed);
#endif
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

// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

#define LOGGER_MODULE Vfs

// include-order workaround: mft.h pulls tinyxml2.h, which must come before
// any header that pulls common/logging.h (vfs.h does)
#include "mft.h"

#include "vfs.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include "http.h"
#include "vfs_fetch.h"
#include "vfs_record.h"
#include "vfs_trap.h"

#include "common.h"
#include <common/logging/logging.h>

#define VFS_PRODUCT_FILE_DEFAULT "uo-legacyrelease.prod"
// the official product manifest (VUOI plan §3.1), used when no explicit URL
#define VFS_PRODUCT_URL_DEFAULT \
    "http://patch.uo.eamythic.com/uopatch-sa/legacyrelease/uo/manifest/uo-legacyrelease.prod"
// background tier fetch+materialize chunking
#define VFS_BOOT_T1_CHUNK 32

static vfs_params s_params;
static VfsPlan s_plan;
static bool s_plan_loaded = false;

// boot state (vfs_boot/vfs_shutdown own these; the background tier and the
// fetch workers only touch vfs_fetch/store/state internals under s_state_mutex)
static std::atomic<bool> s_boot_started{ false };
static std::atomic<bool> s_boot_shutdown{ false };
static std::thread s_background;
static std::mutex s_state_mutex;
static std::vector<uint8_t> s_states;
static vfs_journal s_journal;

// managed-mapping registry (M3): plan file name (lowercased) -> file index.
// Built when the plan is loaded; consumed by vfs_map_file.
static std::map<astr_t, size_t> s_managed_files;
static bool s_managed_built = false;

// ---------------------------------------------------------------------------
// managed mappings (M3, plan §6.1, §6.5)
// ---------------------------------------------------------------------------

static astr_t vfs_lower_ascii(const char *text)
{
    astr_t lower(text);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c)
                   { return char(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c); });
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
    for (size_t i = 0; i < s_plan.files.size(); ++i)
        s_managed_files[vfs_lower_ascii(s_plan.files[i].name.c_str())] = i;
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
// fetch engine dedups equal in-flight tails), then run the M1 write path for
// exactly this asset. Called with no trap lock held (plan §9 lock order).
// The wait is bounded (m3): a fetch stalled past this turns the fault into
// the zero-fill path instead of parking the faulting thread forever
#define VFS_TRAP_FETCH_TIMEOUT_MS (30 * 1000)

static bool vfs_trap_ensure_asset(void *context, const vfs_trap_block *block)
{
    (void)context;
    if (!block || !vfs_plan_ready() || block->file_index >= s_plan.files.size())
        return false;
    const auto &file = s_plan.files[block->file_index];
    if (block->asset_index >= file.assets.size())
        return false;
    const auto &asset = file.assets[block->asset_index];
    if (!s_params.offline && !fs_path_exists(fs_path_join(s_params.blobs_dir, asset.blob)))
    {
        if (!vfs_fetch_started())
            return false; // no engine: nothing can appear in the store
        if (!vfs_fetch_blob_to_store(
                asset.blob.c_str(), block->expected, false, VFS_TRAP_FETCH_TIMEOUT_MS))
            return false; // the fault degrades to the zero-fill path
    }
    vfs_store_params store_params;
    store_params.blobs_dir = s_params.blobs_dir;
    store_params.install_dir = s_params.install_dir;
    store_params.state_dir = s_params.state_dir;
    std::unique_lock<std::mutex> lock(s_state_mutex);
    return vfs_store_rematerialize_asset(
        s_plan, store_params, s_states, &s_journal, block->file_index, block->asset_index);
}

static void vfs_trap_backend_ensure()
{
    vfs_trap_backend backend;
    backend.context = nullptr;
    backend.ensure = vfs_trap_ensure_asset;
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
            // is never held across the trap's own lock (plan §9 order)
            std::vector<uint8_t> states;
            const size_t assets = s_plan.files[size_t(index)].assets.size();
            {
                std::unique_lock<std::mutex> lock(s_state_mutex);
                const size_t base = vfs_state_base_index(s_plan, size_t(index));
                if (base < s_states.size())
                {
                    const size_t count = std::min(assets, s_states.size() - base);
                    states.assign(s_states.begin() + ptrdiff_t(base),
                                  s_states.begin() + ptrdiff_t(base + count));
                }
            }
            out_start = vfs_trap_map(
                path,
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
// params
// ---------------------------------------------------------------------------

void vfs_set_params(const vfs_params &params)
{
    s_params = params;
    s_plan_loaded = false;
    s_managed_built = false; // the registry follows the (new) plan
    s_states.clear();        // runtime state binds to the previous params
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
static bool vfs_product_serial(const fs_path &product_file, uint64_t *out_serial)
{
    std::vector<uint8_t> data;
    if (!fs_file_read(fs_path_ascii(product_file), data))
        return false;
    tinyxml2::XMLDocument doc;
    doc.Parse(reinterpret_cast<char *>(data.data()), data.size());
    auto root = doc.FirstChildElement("MythicMFT");
    if (!root)
        return false;
    auto product = root->FirstChildElement("product");
    if (!product)
        return false;
    *out_serial = product->Unsigned64Attribute("serial", 0);
    return true;
}

bool vfs_plan_rebuild(VfsPlan &out_plan)
{
    if (!vfs_params_ready())
        return false;
    vfs_manifest_params params;
    params.product_file = s_params.product_file;
    params.blobs_dir = s_params.blobs_dir;
    if (!vfs_manifest_build(params, out_plan))
        return false;
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
    vfs_state_reconcile(plan, s_params.install_dir, states, &reconcile);
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
    vfs_record_close();
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
    return vfs_store_verify(plan, store_params, out);
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
        vfs_state_reconcile(plan, s_params.install_dir, states, nullptr);

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
    if (out)
        *out = stats;
}

// ---------------------------------------------------------------------------
// boot (M2, plan §7.2)
// ---------------------------------------------------------------------------

// one asset the boot pipeline may need: its fetch identity (URL tail + wire
// size) and its write identity (file/asset indexes for the store)
struct vfs_boot_item
{
    size_t file_index = 0;
    size_t asset_index = 0;
    size_t global_index = 0;
    astr_t tail;         // blob store / URL tail (mft_entry_remote_path form)
    uint64_t expected = 0; // exact on-wire size (0: unknown)
    uint8_t tier = vfs_tier_t2;
    astr_t name; // for failure logs ("tiledata.mul" / pack name)
};

// collects every not-yet-fetched, non-hole asset of `tier`; `fetch_list`
// gets the ones whose blob is not in the store yet (the fetch list)
static void vfs_boot_collect(
    const VfsPlan &plan,
    uint8_t tier,
    std::vector<vfs_boot_item> *items,
    std::vector<vfs_boot_item> *fetch_list,
    size_t *holes)
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
            if (items && gi < s_states.size() && s_states[gi] != vfs_fetched)
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

// §7.5: the embedded (recorded) manifest orders the T1 tier; assets it does
// not mention keep plan order after the recorded ones. Stale entries only
// reorder - they can never break correctness.
static void vfs_boot_order_t1(const VfsPlan &plan, std::vector<vfs_boot_item> *items)
{
    const auto &embedded = vfs_manifest_embedded();
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

// materializes the given items through the M1 write path (subset run); a
// null `items` materializes everything reachable from the store
static bool vfs_boot_materialize(
    const VfsPlan &plan,
    const std::vector<vfs_boot_item> *items,
    vfs_materialize_stats *stats)
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

    std::unique_lock<std::mutex> lock(s_state_mutex);
    const std::vector<vfs_store_item> *list = items != nullptr ? &store_items : nullptr;
    return vfs_store_materialize(plan, store_params, s_states, &s_journal, stats, list);
}

// downloads the product manifest into the blob store when missing (online
// boot); a local --product file always wins
static bool vfs_boot_product_fetch()
{
    if (fs_path_exists(s_params.product_file))
        return true;
    if (s_params.offline)
    {
        LOG_ERROR("vfs: offline and no product manifest: %s", fs_path_ascii(s_params.product_file));
        return false;
    }
    LOG_INFO("fetching product manifest: %s", s_params.product_url.c_str());
    std::vector<uint8_t> data;
    if (!http_get_binary(s_params.product_url.c_str(), data))
    {
        LOG_ERROR("vfs: could not download the product manifest");
        return false;
    }
    fs_path_create(s_params.blobs_dir);
    if (!fs_file_write(fs_path_ascii(s_params.product_file), data))
    {
        LOG_ERROR("vfs: could not store the product manifest");
        return false;
    }
    return true;
}

// plan acquisition for boot: persisted plan when the product serial matches,
// (re)build otherwise - online through the HTTP manifest seam, offline from
// the store. Sets the fetch engine's repos either way.
static bool vfs_boot_plan(VfsPlan &out_plan)
{
    if (!vfs_params_dirs_ready())
        return false;

    if (!vfs_boot_product_fetch())
        return false;

    const auto plan_path = fs_path_join(s_params.state_dir, "plan.bin");
    uint64_t serial = 0;
    if (vfs_plan_load(plan_path, out_plan) &&
        vfs_product_serial(s_params.product_file, &serial) && serial == out_plan.product_serial)
    {
        s_plan = out_plan;
        s_plan_loaded = true;
        LOG_DEBUG("plan loaded: %s", fs_path_ascii(plan_path));
        vfs_fetch_set_repos(out_plan.manifest_repo.c_str(), out_plan.file_repo.c_str());
        return true;
    }

    vfs_manifest_params params;
    params.product_file = s_params.product_file;
    params.blobs_dir = s_params.blobs_dir;
    if (!s_params.offline && vfs_fetch_started())
    {
        // manifest recursion over HTTP (downloads land in the store)
        params.fetch = vfs_fetch_manifest_fn;
        params.fetch_context = nullptr;
    }
    if (!vfs_manifest_build(params, out_plan))
        return false;
    if (!vfs_plan_save(plan_path, out_plan))
        return false;
    s_plan = out_plan;
    s_plan_loaded = true;
    vfs_fetch_set_repos(out_plan.manifest_repo.c_str(), out_plan.file_repo.c_str());
    return true;
}

// the background tier: T1 fetch (ordered) + materialize in chunks; runs until
// the tier completes or shutdown is requested. Failed blobs are logged and
// stay missing (retried on the next boot, §11).
static void vfs_boot_background(VfsPlan plan, std::vector<vfs_boot_item> items)
{
    LOG_INFO("background tier: %zu assets", items.size());
    for (size_t pos = 0; pos < items.size() && !s_boot_shutdown;)
    {
        const size_t count = std::min(size_t(VFS_BOOT_T1_CHUNK), items.size() - pos);
        const auto first = items.begin() + ptrdiff_t(pos);
        const auto last = first + ptrdiff_t(count);
        const std::vector<vfs_boot_item> chunk(first, last);
        if (!s_params.offline)
        {
            std::vector<astr_t> tails;
            std::vector<uint64_t> sizes;
            vfs_boot_items_to_tails(chunk, &tails, &sizes);
            vfs_fetch_batch(tails, sizes, 1, false, nullptr);
        }
        if (s_boot_shutdown)
            break;
        vfs_boot_materialize(plan, &chunk, nullptr);
        pos += count;
    }
    if (!s_boot_shutdown)
    {
        LOG_INFO("background tier complete");
        // §5.2 cache rule: everything already in the store materializes
        // without the network (T2+ included), so the install never keeps
        // sized-but-unfetched files when the blob is available locally
        vfs_materialize_stats stats;
        vfs_boot_materialize(plan, nullptr, &stats);
    }
    // fold point: flush the tier's progress into state.bin
    std::unique_lock<std::mutex> lock(s_state_mutex);
    vfs_state_close(plan, s_states, s_journal);
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
    if (!vfs_boot_plan(plan))
    {
        vfs_fetch_stop();
        return false;
    }
    vfs_manifest_assign_tiers(plan);

    // state db: load + journal + reconcile (M1), then the boot skeleton
    const size_t asset_count = vfs_state_asset_count(plan);
    {
        std::unique_lock<std::mutex> lock(s_state_mutex);
        s_states.clear();
        vfs_state_load(fs_path_join(s_params.state_dir, "state.bin"), asset_count, s_states);
        std::vector<vfs_journal_record> records;
        vfs_journal_replay(fs_path_join(s_params.state_dir, "journal.log"), records);
        vfs_state_apply_journal(plan, records, s_states);
        vfs_reconcile_stats reconcile;
        vfs_state_reconcile(plan, s_params.install_dir, s_states, &reconcile);
        if (reconcile.assets_repaired)
        {
            LOG_INFO(
                "reconcile: %zu of %zu assets downgraded to pending",
                reconcile.assets_repaired,
                reconcile.assets_checked);
        }
        if (!vfs_journal_open(s_journal, fs_path_join(s_params.state_dir, "journal.log")))
        {
            vfs_fetch_stop();
            return false;
        }
    }

    vfs_store_params store_params;
    store_params.blobs_dir = s_params.blobs_dir;
    store_params.install_dir = s_params.install_dir;
    store_params.state_dir = s_params.state_dir;
    if (!vfs_store_prepare(plan, store_params))
    {
        LOG_ERROR("vfs: could not prepare the install skeleton");
        vfs_fetch_stop();
        return false;
    }
    fs_path_create(s_params.install_dir);

    // managed-mapping registry: file -> plan index, consumed by vfs_map_file
    vfs_managed_build();

    // tier work lists
    std::vector<vfs_boot_item> t0, t0_fetch, t1, t1_fetch;
    size_t holes = 0, t2 = 0, t3 = 0;
    vfs_boot_collect(plan, vfs_tier_t0, &t0, &t0_fetch, &holes);
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
    vfs_boot_materialize(plan, &t0, &stats);

    // every core asset must be in place: a missing T0 file is a boot failure
    // naming the file (§11)
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
        if (!failed)
        {
            // fold point for the blocking tier
            vfs_state_close(plan, s_states, s_journal);
            vfs_journal_open(s_journal, fs_path_join(s_params.state_dir, "journal.log"));
        }
    }
    if (failed)
    {
        LOG_ERROR("vfs: boot failed in the blocking tier");
        vfs_fetch_stop();
        vfs_runtime_close(); // journal + recorder were opened before T0
        if (progress)
            progress(1.0f, "failed");
        return false;
    }
    if (progress)
        progress(1.0f, "starting");

    // trap backend (M3): bound before any managed mapping can exist; the
    // fault handler itself installs lazily with the first vfs_trap_map, so a
    // run that never maps a managed file never installs one
    vfs_trap_backend_ensure();

    // T1: background worker loop (never blocks the game render loop)
    s_boot_shutdown = false;
    s_boot_started = true;
    s_background = std::thread(vfs_boot_background, plan, std::move(t1));
    return true;
}

void vfs_shutdown()
{
    // the trap goes first: no resolution may run against a torn-down store
    // (later faults outside managed ranges chain to the default crash)
    vfs_trap_shutdown();
    if (s_boot_started)
    {
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
    if (s_background.joinable())
        s_background.join();
}

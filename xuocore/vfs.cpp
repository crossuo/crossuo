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

// ---------------------------------------------------------------------------
// params
// ---------------------------------------------------------------------------

void vfs_set_params(const vfs_params &params)
{
    s_params = params;
    s_plan_loaded = false;
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

    // T1: background worker loop (never blocks the game render loop)
    s_boot_shutdown = false;
    s_boot_started = true;
    s_background = std::thread(vfs_boot_background, plan, std::move(t1));
    return true;
}

void vfs_shutdown()
{
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

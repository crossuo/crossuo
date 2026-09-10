// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// web-groundwork tests (all verified on native):
//   - the store_arena backend through the store_sparse core battery re-mixed
//     for arena semantics (ensure/is_ready/read, prefetch + demand heal,
//     LRU eviction, seal, zerolat stub+heal)
//   - the product-subset filter incl. plan.bin round-trip and the
//     drift rebuild re-applying it
//   - the transport seam: a fake transport runs the FULL fetch pipeline
//     (manifests + blobs, no curl, no network) and the fixture server's
//     COOP/COEP headers are on the wire

#define LOGGER_MODULE TestVfs

#include <stdio.h>
#include <string.h>

#include <stddef.h>
#include <atomic>
#include <map>
#include <string>
#include <vector>

#include "test_main.h"
#include "test_product.h"
#include "test_server.h"

#include <curl/curl.h>

#include <xuocore/common.h>
#include <common/logging/logging.h>
#include <xuocore/uodata.h>
#include <xuocore/uop.h>
#include <xuocore/vfs.h>
#include <xuocore/vfs_fetch.h>
#include <xuocore/vfs_store_arena.h>
#include <xuocore/vfs_stub.h>
#include <xuocore/vfs_transport.h>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

struct arena_log_mute
{
    eLogSystem saved;
    arena_log_mute()
        : saved(g_LogEnabled)
    {
        g_LogEnabled = eLogSystem::LogSystemNone;
    }
    ~arena_log_mute() { g_LogEnabled = saved; }
};

static bool test_build_plan(const test_product_data &prod, VfsPlan &plan)
{
    vfs_manifest_params manifest_params;
    manifest_params.product_file = fs_path_join(prod.root, "prod.xml");
    manifest_params.blobs_dir = prod.blobs;
    return vfs_manifest_build(manifest_params, plan);
}

static vfs_store_arena_params test_arena_params(const test_product_data &prod)
{
    vfs_store_arena_params params;
    params.store.blobs_dir = prod.blobs;
    return params;
}

static const VfsAsset *test_loose_asset(const VfsPlan &plan, const char *name)
{
    for (const auto &file : plan.files)
    {
        if (!file.is_uop && file.name == name && !file.assets.empty())
            return &file.assets[0];
    }
    return nullptr;
}

// waits (bounded) for a store blob to appear (the demand/prefetch queues
// settle asynchronously)
static bool test_wait_for_blob(const fs_path &blobs, const VfsAsset &asset)
{
    const auto path = fs_path_join(blobs, asset.blob);
    for (int i = 0; i < 200 && !fs_path_exists(path); ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return fs_path_exists(path);
}

static bool test_read_file(const fs_path &path, std::vector<uint8_t> &out)
{
    FILE *fp = fopen(fs_path_ascii(path), "rb");
    if (fp == nullptr)
    {
        return false;
    }
    fseek(fp, 0, SEEK_END);
    const long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    out.resize(size_t(size));
    const size_t read = fread(out.data(), 1, out.size(), fp);
    fclose(fp);
    return read == out.size();
}

// ---------------------------------------------------------------------------
// backend factory + the core battery (ensure / is_ready / read round-trip)
// ---------------------------------------------------------------------------

VFS_TEST(arena_backend_factory)
{
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "arena_factory"));
    VfsPlan plan;
    TEST_CHECK(test_build_plan(prod, plan));

    vfs_store_params store_params;
    store_params.blobs_dir = prod.blobs;
    store_params.install_dir = prod.install;

    // default kind: the native sparse backend (unchanged)
    vfs_store_backend *sparse = vfs_store_backend_create(vfs_store_backend_sparse, store_params);
    TEST_CHECK(sparse != nullptr);
    TEST_CHECK(sparse->open(plan));
    delete sparse;

    // arena kind: the memory backend, params feeding only the blob store
    vfs_store_backend *arena = vfs_store_backend_create(vfs_store_backend_arena, store_params);
    TEST_CHECK(arena != nullptr);
    TEST_CHECK(arena->open(plan));
    delete arena;
}

VFS_TEST(arena_ensure_read_roundtrip)
{
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "arena_roundtrip"));
    VfsPlan plan;
    TEST_CHECK(test_build_plan(prod, plan));

    vfs_store_arena arena(test_arena_params(prod));
    TEST_CHECK_MSG(arena.open(plan), "arena open failed");

    const auto &part0 = plan.files[0].assets[0]; // zlib + metadata
    const auto &part1 = plan.files[0].assets[1]; // stored
    const auto &hole = plan.files[0].assets[3];  // blockless entry
    const auto *tiledata = test_loose_asset(plan, "tiledata.mul");
    const auto *music = test_loose_asset(plan, "Music/Digital/Britainpos.mp3");
    TEST_CHECK(tiledata != nullptr && music != nullptr);

    // nothing is resident before the first ensure
    TEST_CHECK(!arena.is_ready(part0));
    arena.evict(0); // no-op: nothing resident

    // zlib pack part: ensure -> ready -> exact payload bytes
    TEST_CHECK_MSG(arena.ensure(part0, true), "ensure failed for the zlib part");
    TEST_CHECK(arena.is_ready(part0));
    std::vector<uint8_t> bytes(prod.parts[0].payload.size() + 16);
    TEST_CHECK_MSG(
        arena.read(part0, 0, bytes.data(), prod.parts[0].payload.size()),
        "read failed for the zlib part");
    bytes.resize(prod.parts[0].payload.size());
    TEST_CHECK(bytes == prod.parts[0].payload);

    // explicit ranges read the same resident payload (the arena has no
    // traps: the caller supplies the buffer)
    uint8_t slice[32] = {};
    TEST_CHECK(arena.read(part0, 10, slice, sizeof(slice)));
    TEST_CHECK(memcmp(slice, prod.parts[0].payload.data() + 10, sizeof(slice)) == 0);
    // out-of-range reads fail cleanly, never out of bounds
    TEST_CHECK(!arena.read(part0, prod.parts[0].payload.size() - 1, slice, sizeof(slice)));
    TEST_CHECK(!arena.read(part0, prod.parts[0].payload.size() + 1, slice, 1));

    // stored pack part: raw payload bytes
    TEST_CHECK(arena.ensure(part1, true));
    TEST_CHECK(arena.read(part1, 0, slice, sizeof(slice)));
    TEST_CHECK(memcmp(slice, prod.parts[1].payload.data(), sizeof(slice)) == 0);

    // the zlib part with the big metadata: same payload residency
    const auto &part2 = plan.files[0].assets[2];
    TEST_CHECK(arena.ensure(part2, true));
    std::vector<uint8_t> part2_bytes(prod.parts[2].payload.size());
    TEST_CHECK(arena.read(part2, 0, part2_bytes.data(), part2_bytes.size()));
    TEST_CHECK(part2_bytes == prod.parts[2].payload);

    // the hole: trivially ready (sparse parity), but there are no bytes
    TEST_CHECK(arena.ensure(hole, true));
    TEST_CHECK(arena.is_ready(hole));
    TEST_CHECK(!arena.read(hole, 0, slice, 1));

    // loose files: stored + zlib, whole-payload residency
    TEST_CHECK(arena.ensure(*tiledata, true));
    std::vector<uint8_t> loose_bytes(prod.loose[0].payload.size());
    TEST_CHECK(arena.read(*tiledata, 0, loose_bytes.data(), loose_bytes.size()));
    TEST_CHECK(loose_bytes == prod.loose[0].payload);
    TEST_CHECK(arena.ensure(*music, true));
    loose_bytes.resize(prod.loose[1].payload.size());
    TEST_CHECK(arena.read(*music, 0, loose_bytes.data(), loose_bytes.size()));
    TEST_CHECK(loose_bytes == prod.loose[1].payload);

    // stats: 5 payload granules (6 assets - the hole), all resident, none
    // evicted; resident bytes == the payload total (tables are not resident)
    const vfs_arena_stats stats = arena.stats();
    TEST_CHECK(stats.granules == 5);
    TEST_CHECK(stats.resident == 5);
    TEST_CHECK(stats.stubbed == 0);
    TEST_CHECK(stats.evictions == 0);
    uint64_t payload_total = 0;
    for (const auto &part : prod.parts)
    {
        payload_total += part.payload.size();
    }
    payload_total += prod.loose[0].payload.size();
    payload_total += prod.loose[1].payload.size();
    TEST_CHECK(stats.bytes_resident == payload_total);

    // seal: every granule of pack0 is resident -> the file is sealed; the
    // loose "files" seal the same way
    arena.seal_check();
    TEST_CHECK(arena.file_sealed(0));
}

VFS_TEST(arena_eviction_lru_order)
{
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "arena_evict"));
    VfsPlan plan;
    TEST_CHECK(test_build_plan(prod, plan));

    // budget fits the touched part0 plus the incoming part2 exactly: after
    // the touch below, the insertion must evict exactly ONE granule - and
    // the LRU rule decides which one (part2's payload outweighs part1's, so
    // a part0+part1 budget would force a second, byte-driven eviction)
    vfs_store_arena_params params = test_arena_params(prod);
    params.max_bytes = prod.parts[0].payload.size() + prod.parts[2].payload.size();
    vfs_store_arena arena(params);
    TEST_CHECK(arena.open(plan));

    const auto &part0 = plan.files[0].assets[0];
    const auto &part1 = plan.files[0].assets[1];
    const auto &part2 = plan.files[0].assets[2];

    TEST_CHECK(arena.ensure(part0, true));
    TEST_CHECK(arena.ensure(part1, true));
    TEST_CHECK(arena.stats().evictions == 0); // at the budget: nothing to drop

    // the LRU-vs-FIFO discriminator: a read of part0 refreshes its recency,
    // so the eviction below must take part1 (FIFO would drop part0, the
    // first-inserted granule)
    uint8_t slice[8] = {};
    TEST_CHECK(arena.read(part0, 0, slice, sizeof(slice)));
    TEST_CHECK(arena.ensure(part2, true)); // pushes the oldest out
    TEST_CHECK(arena.is_ready(part0));     // touched: survives the eviction
    TEST_CHECK(!arena.is_ready(part1));    // part 1 was the LRU victim
    TEST_CHECK(arena.is_ready(part2));
    TEST_CHECK(arena.stats().evictions == 1);

    // a larger-than-resident target is a no-op ...
    const uint64_t before = arena.stats().evictions;
    arena.evict(params.max_bytes * 2);
    TEST_CHECK(arena.stats().evictions == before);
    // ... and so is a repeat ensure of a resident granule (touch, not evict)
    TEST_CHECK(arena.ensure(part0, true));
    TEST_CHECK(arena.stats().evictions == before);

    // budget 0 evicts everything evictable (no in-flight ensures here)
    arena.evict(0);
    TEST_CHECK(!arena.is_ready(part0));
    TEST_CHECK(!arena.is_ready(part2));
    const vfs_arena_stats stats = arena.stats();
    TEST_CHECK(stats.resident == 0);
    TEST_CHECK(stats.bytes_resident == 0);
    TEST_CHECK(stats.evictions == before + 2); // part0 + part2 (part1 went earlier)

    // evicted granules re-ensure from the store (bytes are recoverable)
    TEST_CHECK(arena.ensure(part1, true));
    TEST_CHECK(arena.read(part1, 0, slice, sizeof(slice)));
    TEST_CHECK(memcmp(slice, prod.parts[1].payload.data(), sizeof(slice)) == 0);
}

VFS_TEST(arena_stub_fill_and_heal)
{
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "arena_stub"));
    // the third part's blob is missing: the zerolat analog must fill the
    // arena with the stub payload, then heal when the blob arrives
    const auto &missing = prod.parts[2];
    fs_del(fs_path_join(prod.blobs, missing.blob_rel));

    VfsPlan plan;
    TEST_CHECK(test_build_plan(prod, plan));

    vfs_store_arena_params params = test_arena_params(prod);
    params.stub_on_miss = true;
    vfs_store_arena arena(params);
    TEST_CHECK(arena.open(plan));

    const auto &part2 = plan.files[0].assets[2];
    TEST_CHECK(!arena.is_ready(part2));

    // the zero-latency miss path answers instantly: non-blocking ensure fills the
    // arena with the kind stub (same generator as the trap) and reports
    // ready - the caller learns what happened from asset_is_stubbed
    TEST_CHECK_MSG(arena.ensure(part2, false), "stub-on-miss ensure must succeed");
    TEST_CHECK(arena.is_ready(part2));
    TEST_CHECK(arena.asset_is_stubbed(part2));

    std::vector<uint8_t> bytes(missing.payload.size());
    TEST_CHECK(arena.read(part2, 0, bytes.data(), bytes.size()));
    // the expected payload is the pure generator's answer (stub matrix)
    std::vector<uint8_t> expected(missing.payload.size());
    vfs_stub_fill(
        vfs_stub_kind_for("pack0.uop"),
        missing.payload.size(),
        true,
        expected.data(),
        expected.size());
    TEST_CHECK(bytes == expected);

    // heal: the blob lands, ensure overwrites the arena bytes
    TEST_CHECK(test_write_file(fs_path_join(prod.blobs, missing.blob_rel), missing.blob));
    TEST_CHECK(arena.ensure(part2, true));
    TEST_CHECK(!arena.asset_is_stubbed(part2));
    TEST_CHECK(arena.read(part2, 0, bytes.data(), bytes.size()));
    TEST_CHECK(bytes == missing.payload);

    // the stats agree: one stubbed granule healed to a real payload
    TEST_CHECK(arena.stats().stubbed == 0);
    TEST_CHECK(arena.stats().resident == 1);
}

// ---------------------------------------------------------------------------
// prefetch + demand heal over the fixture server (fetch engine running)
// ---------------------------------------------------------------------------

VFS_TEST(arena_prefetch_and_demand_heal)
{
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "arena_prefetch"));

    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));
    VfsPlan plan;
    TEST_CHECK(test_build_plan(prod, plan));

    arena_log_mute mute;
    // the arena reads from an EMPTY store: the blobs arrive over HTTP
    vfs_store_arena_params params = test_arena_params(prod);
    params.store.blobs_dir = fs_path_join(prod.root, "arena-blobs");
    vfs_store_arena arena(params);
    TEST_CHECK(arena.open(plan));

    http_init();
    vfs_fetch_params fetch_params;
    fetch_params.workers = 2;
    TEST_CHECK(vfs_fetch_start(fetch_params));
    vfs_fetch_set_store(params.store.blobs_dir);
    const auto repo = test_server_url(srv, "/blobs/");
    vfs_fetch_set_repos(repo.c_str(), repo.c_str());

    const auto &part0 = plan.files[0].assets[0];
    const auto &part1 = plan.files[0].assets[1];
    const auto *tiledata = test_loose_asset(plan, "tiledata.mul");
    TEST_CHECK(tiledata != nullptr);

    // prefetch queues the granule's blob; the arena heals when it lands
    TEST_CHECK(!arena.is_ready(part0));
    arena.prefetch(part0, 1);
    TEST_CHECK_MSG(test_wait_for_blob(params.store.blobs_dir, part0), "prefetch blob never landed");
    TEST_CHECK_MSG(arena.ensure(part0, true), "heal ensure failed");
    TEST_CHECK(arena.is_ready(part0));
    uint8_t slice[32] = {};
    TEST_CHECK(arena.read(part0, 0, slice, sizeof(slice)));
    TEST_CHECK(memcmp(slice, prod.parts[0].payload.data(), sizeof(slice)) == 0);
    TEST_CHECK(test_server_hits(srv, ("/blobs/" + part0.blob).c_str()) == 1);

    // demand path: a non-blocking ensure reports the miss AND queues the
    // fetch (async model); the next blocking ensure heals
    TEST_CHECK(!arena.ensure(part1, false));
    TEST_CHECK(!arena.is_ready(part1));
    TEST_CHECK_MSG(test_wait_for_blob(params.store.blobs_dir, part1), "demand blob never landed");
    TEST_CHECK(arena.ensure(part1, true));
    TEST_CHECK(arena.is_ready(part1));

    // two racing demand ensures dedup into ONE request
    TEST_CHECK(!arena.ensure(*tiledata, false));
    TEST_CHECK(!arena.ensure(*tiledata, false));
    TEST_CHECK_MSG(
        test_wait_for_blob(params.store.blobs_dir, *tiledata), "loose blob never landed");
    TEST_CHECK(test_server_hits(srv, ("/blobs/" + tiledata->blob).c_str()) == 1);
    TEST_CHECK(arena.ensure(*tiledata, true));
    std::vector<uint8_t> bytes(prod.loose[0].payload.size());
    TEST_CHECK(arena.read(*tiledata, 0, bytes.data(), bytes.size()));
    TEST_CHECK(bytes == prod.loose[0].payload);

    vfs_fetch_stop();
    test_server_stop(srv);
}

// ---------------------------------------------------------------------------
// arena boot (native memory-mode): no files materialize, granules are
// resident, the state DB keeps its meaning
// ---------------------------------------------------------------------------

VFS_TEST(arena_boot_memory_mode)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "arena_boot"));

    arena_log_mute mute;
    vfs_params params;
    params.enabled = true;
    params.offline = true; // full store: no network needed
    params.install_dir = prod.install;
    params.blobs_dir = prod.blobs;
    params.product_file = fs_path_join(prod.root, "prod.xml");
    params.store_backend = vfs_store_backend_arena;
    vfs_set_params(params);

    TEST_CHECK_MSG(vfs_boot(nullptr), "arena boot failed");
    vfs_boot_wait();

    // no install files exist: granules live in the arena
    TEST_CHECK(!fs_path_exists(fs_path_join(prod.install, "pack0.uop")));
    TEST_CHECK(!fs_path_exists(fs_path_join(prod.install, "tiledata.mul")));
    TEST_CHECK(!fs_path_exists(fs_path_join(prod.install, "Music", "Digital", "Britainpos.mp3")));

    // the boot-owned accounting reflects the resident granules
    VfsStats stats;
    vfs_hud_stats(&stats);
    TEST_CHECK(stats.files_total > 0);
    TEST_CHECK_MSG(stats.files_done == stats.files_total, "not every file reached residency");
    TEST_CHECK(stats.bytes_done > 0);

    // a REAL second boot: vfs_boot is a no-op while the first one holds
    // (s_boot_started), so the arena must be torn down first - it comes back
    // empty no matter what state.bin says
    vfs_shutdown();
    TEST_CHECK_MSG(vfs_boot(nullptr), "second arena boot failed");
    vfs_boot_wait();

    // warm-boot residency contract: T0 granules re-ensure (blocking:
    // core and small), T1+ repopulate on access. The advisory state still
    // covers every file, but only the core granule is truly resident after
    // the restart - and the fixture's T0 set is exactly tiledata.mul
    VfsStats warm;
    vfs_hud_stats(&warm);
    TEST_CHECK_MSG(warm.files_done == warm.files_total, "advisory state lost on the warm boot");
    size_t granules = 0, resident = 0;
    vfs_arena_residency(&granules, &resident);
    TEST_CHECK(granules == 5); // 3 pack parts + 2 loose (the hole is untracked)
    TEST_CHECK_MSG(resident == 1, "the warm boot did not restore exactly the T0 residency");

    vfs_shutdown();
}

// ---------------------------------------------------------------------------
// product subsets
// ---------------------------------------------------------------------------

VFS_TEST(subset_web_excludes_patterns)
{
    test_product_data prod;
    // register one file per exclusion pattern plus survivors
    prod.extra_valid_loose.push_back({ "anim.mul", 0, 100 });
    prod.extra_valid_loose.push_back({ "anim2.mul", 0, 100 });
    prod.extra_valid_loose.push_back({ "facet0.mul", 0, 100 });
    prod.extra_valid_loose.push_back({ "verdata.mul", 0, 100 });
    TEST_CHECK(test_build_product(prod, "subset_plan"));

    // the rule itself: exact exclusion patterns, nothing else
    TEST_CHECK(vfs_subset_web_excludes("anim.mul"));
    TEST_CHECK(vfs_subset_web_excludes("anim2.mul"));
    TEST_CHECK(vfs_subset_web_excludes("anim(2).mul")); // the fallback naming
    TEST_CHECK(vfs_subset_web_excludes("facet0.mul"));
    TEST_CHECK(vfs_subset_web_excludes("facetMul.mul"));
    TEST_CHECK(vfs_subset_web_excludes("verdata.mul"));
    TEST_CHECK(vfs_subset_web_excludes("verdata.idx"));
    TEST_CHECK(vfs_subset_web_excludes("Music/Digital/Britainpos.mp3"));
    TEST_CHECK(!vfs_subset_web_excludes("AnimationFrame1.uop")); // UOP anims stay
    TEST_CHECK(!vfs_subset_web_excludes("tiledata.mul"));
    TEST_CHECK(!vfs_subset_web_excludes("statics0.mul"));
    TEST_CHECK(!vfs_subset_web_excludes("artLegacyMUL.uop"));
    TEST_CHECK(!vfs_subset_web_excludes(""));

    VfsPlan plan;
    TEST_CHECK(test_build_plan(prod, plan));
    const size_t full_count = plan.files.size();

    vfs_manifest_apply_subset(plan, vfs_subset_none);
    TEST_CHECK(plan.files.size() == full_count); // none: no-op
    TEST_CHECK(plan.subset == vfs_subset_none);

    vfs_manifest_apply_subset(plan, vfs_subset_web);
    TEST_CHECK(plan.subset == uint8_t(vfs_subset_web));

    // excluded: every pattern file + the default product's Music dir
    const auto has_file = [&plan](const char *name)
    {
        for (const auto &file : plan.files)
        {
            if (file.name == name)
                return true;
        }
        return false;
    };
    TEST_CHECK(!has_file("anim.mul"));
    TEST_CHECK(!has_file("anim2.mul"));
    TEST_CHECK(!has_file("facet0.mul"));
    TEST_CHECK(!has_file("verdata.mul"));
    TEST_CHECK(!has_file("Music/Digital/Britainpos.mp3"));

    // kept: the pack (whole, hole entry included) and the survivors
    TEST_CHECK(has_file("pack0.uop"));
    TEST_CHECK(plan.files[0].is_uop);
    TEST_CHECK(plan.files[0].assets.size() == 4); // 3 parts + the hole
    TEST_CHECK(has_file("tiledata.mul"));

    // idempotent: re-applying changes nothing
    const size_t web_count = plan.files.size();
    vfs_manifest_apply_subset(plan, vfs_subset_web);
    TEST_CHECK(plan.files.size() == web_count);

    // the web plan is a strict subset of the same manifests
    TEST_CHECK(web_count < full_count);
}

VFS_TEST(subset_plan_roundtrip_and_v2_compat)
{
    test_product_data prod;
    prod.extra_valid_loose.push_back({ "anim.mul", 0, 100 });
    TEST_CHECK(test_build_product(prod, "subset_roundtrip"));

    // a filtered plan serializes + reloads with its subset intact
    VfsPlan plan;
    TEST_CHECK(test_build_plan(prod, plan));
    vfs_manifest_apply_subset(plan, vfs_subset_web);
    std::vector<uint8_t> bytes;
    TEST_CHECK(vfs_plan_serialize(plan, bytes));
    VfsPlan decoded;
    TEST_CHECK(vfs_plan_deserialize(bytes.data(), bytes.size(), decoded));
    TEST_CHECK(decoded.subset == uint8_t(vfs_subset_web));
    TEST_CHECK(decoded.files.size() == plan.files.size());
    std::vector<uint8_t> again;
    TEST_CHECK(vfs_plan_serialize(decoded, again));
    TEST_CHECK(again == bytes); // deterministic bytes survive the round-trip
    const auto plan_path = fs_path_join(prod.root, "plan.bin");
    TEST_CHECK(vfs_plan_save(plan_path, plan));
    VfsPlan loaded;
    TEST_CHECK(vfs_plan_load(plan_path, loaded));
    TEST_CHECK(loaded.subset == uint8_t(vfs_subset_web));
    TEST_CHECK(loaded.files.size() == plan.files.size());

    // backward compatibility: a v2 payload is exactly a v3 one
    // minus the trailing subset byte - strip it, stamp version 2, and the
    // plan must load with subset none
    VfsPlan unfiltered;
    TEST_CHECK(test_build_plan(prod, unfiltered));
    TEST_CHECK(vfs_plan_serialize(unfiltered, bytes));
    TEST_CHECK(bytes.back() == vfs_subset_none); // the v3 addition
    std::vector<uint8_t> v2(bytes.begin(), bytes.end() - 1);
    const uint32_t version = 2;
    memcpy(v2.data() + sizeof(uint32_t), &version, sizeof(version));
    TEST_CHECK(vfs_plan_deserialize(v2.data(), v2.size(), decoded));
    TEST_CHECK(decoded.subset == vfs_subset_none); // missing => none
    TEST_CHECK(decoded.files.size() == unfiltered.files.size());
    TEST_CHECK(decoded.manifest_repo == unfiltered.manifest_repo);
}

VFS_TEST(subset_drift_rebuild_reapplies_subset)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    prod.extra_valid_loose.push_back({ "anim.mul", 0, 100 });
    TEST_CHECK(test_build_product(prod, "subset_drift"));

    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));

    arena_log_mute mute;
    vfs_params params;
    params.enabled = true;
    params.install_dir = fs_path_join(prod.root, "subset-install");
    params.blobs_dir = fs_path_join(prod.root, "subset-blobs");
    params.product_url = test_server_url(srv, "/prod.xml");
    params.subset = vfs_subset_web;
    vfs_set_params(params);
    TEST_CHECK_MSG(vfs_boot(nullptr), "subset boot failed");
    vfs_boot_wait();
    vfs_shutdown();

    // the persisted plan carries the subset ...
    VfsPlan persisted;
    TEST_CHECK(vfs_plan_load(fs_path_join(vfs_get_params().state_dir, "plan.bin"), persisted));
    TEST_CHECK(persisted.subset == uint8_t(vfs_subset_web));
    for (const auto &file : persisted.files)
    {
        TEST_CHECK_MSG(
            !vfs_subset_web_excludes(file.name), "subset plan kept %s", file.name.c_str());
    }
    // ... and the skeleton never prepared an excluded file
    TEST_CHECK(!fs_path_exists(fs_path_join(params.install_dir, "anim.mul")));
    TEST_CHECK(
        !fs_path_exists(fs_path_join(params.install_dir, "Music", "Digital", "Britainpos.mp3")));
    TEST_CHECK(fs_path_exists(fs_path_join(params.install_dir, "tiledata.mul")));

    // a serial drift rebuilds the plan - the subset must survive the rebuild
    const auto prod_path = fs_path_join(prod.root, "prod.xml");
    std::vector<uint8_t> xml;
    TEST_CHECK(fs_file_read(fs_path_ascii(prod_path), xml));
    astr_t text(xml.begin(), xml.end());
    const auto serial = astr_t("serial=\"") + std::to_string(prod.serial) + "\"";
    const size_t at = text.find(serial);
    TEST_CHECK_MSG(at != astr_t::npos, "serial attribute not found");
    text.replace(at, serial.size(), astr_t("serial=\"") + std::to_string(prod.serial + 1) + "\"");
    TEST_CHECK(test_write_file(prod_path, std::vector<uint8_t>(text.begin(), text.end())));

    vfs_set_params(params);
    TEST_CHECK_MSG(vfs_boot(nullptr), "drift boot failed");
    vfs_boot_wait();
    TEST_CHECK(vfs_plan_load(fs_path_join(vfs_get_params().state_dir, "plan.bin"), persisted));
    TEST_CHECK(persisted.product_serial == prod.serial + 1); // the rebuild ran
    TEST_CHECK(persisted.subset == uint8_t(vfs_subset_web)); // ... and re-applied
    for (const auto &file : persisted.files)
    {
        TEST_CHECK_MSG(
            !vfs_subset_web_excludes(file.name), "drifted plan kept %s", file.name.c_str());
    }
    vfs_shutdown();
    test_server_stop(srv);
}

// the persisted-subset policy: a plan.bin whose stored subset
// differs from the params subset is drift in BOTH directions - the plan must
// rebuild with the params view, and the asset-count change must reset the
// state DB (whose stale journal then gets truncated, not replayed)
VFS_TEST(subset_policy_drift_rebuilds_plan)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    prod.extra_valid_loose.push_back({ "anim.mul", 0, 100 }); // a web exclusion
    TEST_CHECK(test_build_product(prod, "subset_policy"));

    arena_log_mute mute;
    vfs_params params;
    params.enabled = true;
    params.offline = true; // full store: the boots only rewrite the plan/state
    params.install_dir = fs_path_join(prod.root, "policy-install");
    params.state_dir = fs_path_join(prod.root, "policy-state");
    params.blobs_dir = prod.blobs;
    params.product_file = fs_path_join(prod.root, "prod.xml");
    params.store_backend = vfs_store_backend_arena;
    const auto plan_path = fs_path_join(params.state_dir, "plan.bin");

    VfsPlan persisted;
    const auto boot_view = [&](vfs_subset subset)
    {
        params.subset = subset;
        vfs_set_params(params);
        TEST_CHECK_MSG(vfs_boot(nullptr), "boot failed");
        vfs_boot_wait();
        size_t granules = 0, resident = 0;
        vfs_arena_residency(&granules, &resident);
        TEST_CHECK_MSG(
            granules > 0 && resident == granules,
            "a stale state mark survived the subset switch");
        TEST_CHECK(vfs_plan_load(plan_path, persisted));
        vfs_shutdown();
    };

    // web -> none: the stored web view must not shadow the params subset
    boot_view(vfs_subset_web);
    TEST_CHECK(persisted.subset == uint8_t(vfs_subset_web));
    TEST_CHECK(persisted.files.size() == 2); // pack0 + tiledata (music/anim excluded)

    // simulate a crash before the next rotation: a stale fetched mark in the
    // journal must not survive the switch - the cleared state flushes BEFORE
    // the journal truncate (rotation rule), so the replay sees nothing
    {
        FILE *fp = fopen(fs_path_ascii(fs_path_join(params.state_dir, "journal.log")), "a");
        TEST_CHECK(fp != nullptr);
        fputs("F 0 0\n", fp); // pack0 part0: would skip its re-ensure if replayed
        fclose(fp);
    }

    boot_view(vfs_subset_none);
    TEST_CHECK(persisted.subset == vfs_subset_none);
    bool has_music = false, has_anim = false;
    for (const auto &file : persisted.files)
    {
        has_music = has_music || file.name == "Music/Digital/Britainpos.mp3";
        has_anim = has_anim || file.name == "anim.mul";
    }
    TEST_CHECK_MSG(has_music && has_anim, "the web view shadowed the none params");

    // none -> web: the reverse switch drifts the same way
    boot_view(vfs_subset_web);
    TEST_CHECK(persisted.subset == uint8_t(vfs_subset_web));
    for (const auto &file : persisted.files)
    {
        TEST_CHECK_MSG(!vfs_subset_web_excludes(file.name), "plan kept %s", file.name.c_str());
    }
}

// ---------------------------------------------------------------------------
// transport seam: a fake transport runs the FULL pipeline
// ---------------------------------------------------------------------------

namespace
{
struct fake_transport_stats
{
    std::map<std::string, int> calls;
    int manifests = 0;
    int blobs = 0;

    int total() const
    {
        int sum = 0;
        for (const auto &call : calls)
        {
            sum += call.second;
        }
        return sum;
    }
};

// serves canned bytes per resource tail, preloaded from a blob store
// directory - no curl, no sockets, no server
struct fake_transport final : public vfs_transport
{
    std::map<std::string, std::vector<uint8_t>> canned;
    fake_transport_stats stats;

    bool load_tree(const fs_path &root)
    {
        std::vector<fs_path> files;
        fs_path_list(root, files);
        const std::string prefix = fs_path_ascii(root);
        for (const auto &file : files)
        {
            std::string path = fs_path_ascii(file);
            if (path.rfind(prefix, 0) == 0)
            {
                path = path.substr(prefix.size());
            }
            while (!path.empty() && (path[0] == '/' || path[0] == '\\'))
            {
                path.erase(0, 1); // the tail form has no leading separator
            }
            for (char &c : path)
            {
                if (c == '\\')
                {
                    c = '/';
                }
            }
            std::vector<uint8_t> bytes;
            if (!fs_file_read(fs_path_ascii(file), bytes))
            {
                return false;
            }
            canned[path] = std::move(bytes);
        }
        return true;
    }

    vfs_transport_result fetch(
        const char *url,
        const char *tail,
        uint64_t expected_size,
        vfs_transport_sink sink,
        void *sink_context,
        uint32_t timeout_ms,
        bool manifest) override
    {
        (void)url; // the fake is keyed by the store identity
        (void)expected_size;
        (void)timeout_ms;
        vfs_transport_result result;
        stats.calls[tail ? tail : ""]++;
        if (manifest)
        {
            stats.manifests++;
        }
        else
        {
            stats.blobs++;
        }
        const auto found = canned.find(tail ? tail : "");
        if (found == canned.end())
        {
            result.status = 404;
            return result;
        }
        if (sink(sink_context, found->second.data(), found->second.size()))
        {
            result.status = 200;
            result.ok = true;
            result.bytes = found->second.size();
        }
        return result;
    }
};
} // namespace

namespace
{
// transport that parks every request until released: lets a test hold one
// granule's blocking ensure() deterministically in flight (no sockets)
struct gated_transport final : public vfs_transport
{
    std::vector<uint8_t> payload; // the single blob body it serves
    std::atomic<bool> entered{ false };
    std::atomic<bool> release{ false };

    vfs_transport_result fetch(
        const char *url,
        const char *tail,
        uint64_t expected_size,
        vfs_transport_sink sink,
        void *sink_context,
        uint32_t timeout_ms,
        bool manifest) override
    {
        (void)url;
        (void)tail;
        (void)expected_size;
        (void)timeout_ms;
        (void)manifest;
        entered = true; // the ensure() that led here is in flight by now
        while (!release)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        vfs_transport_result result;
        if (sink(sink_context, payload.data(), payload.size()))
        {
            result.status = 200;
            result.ok = true;
            result.bytes = payload.size();
        }
        return result;
    }
};
} // namespace

// THE arena rule (see docs/NOTES.md): a granule with an in-flight ensure() is
// never evicted - the commit would resurrect bytes the budget asked to drop
VFS_TEST(arena_inflight_ensure_survives_eviction)
{
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "arena_inflight"));

    // the arena reads from an EMPTY store: the heal below must fetch, and the
    // gated transport parks that fetch mid-flight
    vfs_store_arena_params params = test_arena_params(prod);
    params.store.blobs_dir = fs_path_join(prod.root, "inflight-blobs");
    params.stub_on_miss = true;
    vfs_store_arena arena(params);
    VfsPlan plan;
    TEST_CHECK(test_build_plan(prod, plan));
    TEST_CHECK(arena.open(plan));

    gated_transport gated;
    gated.payload = prod.parts[2].blob;
    vfs_transport *previous = vfs_transport_set(&gated);

    const auto &part2 = plan.files[0].assets[2];
    // stub fill first - with the engine NOT running, so the fill cannot
    // queue its own demand fetch (that would park in the gate below instead
    // of the heal fetch): the granule is resident (placeholder) when the
    // heal starts, which is exactly the resurrect-on-commit race
    TEST_CHECK(arena.ensure(part2, false));
    TEST_CHECK(arena.asset_is_stubbed(part2));

    http_init(); // engine-level use: this test owns its curl globals
    vfs_fetch_params fetch_params;
    fetch_params.workers = 1;
    TEST_CHECK(vfs_fetch_start(fetch_params));
    vfs_fetch_set_store(params.store.blobs_dir);
    vfs_fetch_set_repos("http://unused.test/", "http://unused.test/");

    // probe state is collected before any TEST_CHECK can return mid-flight
    // (a failing check must never abandon a joinable thread)
    std::atomic<bool> healed{ false };
    std::thread healer([&] { healed = arena.ensure(part2, true); });
    for (int i = 0; i < 2000 && !gated.entered; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    const bool entered = gated.entered;
    // the granule is in flight: the budget must skip it entirely
    arena.evict(0);
    const size_t resident_during = arena.stats().resident;
    const uint64_t evictions_during = arena.stats().evictions;

    gated.release = true;
    healer.join();

    TEST_CHECK_MSG(entered, "the heal fetch never reached the transport");
    TEST_CHECK_MSG(resident_during == 1, "the in-flight granule was dropped by the budget");
    TEST_CHECK_MSG(evictions_during == 0, "an in-flight granule was evicted");
    TEST_CHECK_MSG(healed, "the heal ensure failed");
    TEST_CHECK(!arena.asset_is_stubbed(part2)); // the heal overwrote the stub
    std::vector<uint8_t> bytes(prod.parts[2].payload.size());
    TEST_CHECK(arena.read(part2, 0, bytes.data(), bytes.size()));
    TEST_CHECK(bytes == prod.parts[2].payload);

    vfs_fetch_stop();
    TEST_CHECK(vfs_transport_set(previous) == &gated);
}

VFS_TEST(transport_injection_full_pipeline)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    // the first part becomes the Asset::Strings hash => a T0 asset, so the
    // blocking tier must fetch a pack part through the seam (pack parts are
    // otherwise T2 and only fetched on demand, which the trap owns natively)
    prod.pack_name = "MainMisc.uop";
    prod.first_part_ph = 0x9612dc85;
    prod.first_part_sh = 0x00e53aa2;
    prod.extra_valid_loose.push_back({ "unifont1.mul", 1, 1024 }); // T1 static set
    TEST_CHECK(test_build_product(prod, "transport_fake"));

    arena_log_mute mute;
    fake_transport fake;
    TEST_CHECK_MSG(fake.load_tree(prod.blobs), "could not preload the canned blobs");

    vfs_params params;
    params.enabled = true; // online, but the repos are unreachable fakes
    params.install_dir = fs_path_join(prod.root, "fake-install");
    params.blobs_dir = fs_path_join(prod.root, "fake-blobs"); // empty store
    // the explicit product file skips the product refresh (its http client
    // is outside the transport seam): everything else goes through the fake
    params.product_file = fs_path_join(prod.root, "prod.xml");
    vfs_set_params(params);

    // inject: the previous override is returned so the test unwinds it
    vfs_transport *previous = vfs_transport_set(&fake);
    TEST_CHECK(previous == nullptr); // the curl default was active
    TEST_CHECK(vfs_transport_active() == &fake);

    TEST_CHECK_MSG(vfs_boot(nullptr), "boot with the fake transport failed");
    vfs_boot_wait();

    // the full pipeline ran on the seam: manifests (the product recursion)
    // AND blobs (T0 + background tiers) both honored the injected transport;
    // with the curl transport still wired in, the "*.test" repos would have
    // failed every request and the boot with it
    TEST_CHECK_MSG(fake.stats.manifests > 0, "the manifest seam did not use the transport");
    TEST_CHECK_MSG(fake.stats.blobs > 0, "the blob fetches did not use the transport");
    // the T0 pack part AND the T1 loose file came through the seam
    const auto part0 = fake.stats.calls.find(prod.parts[0].blob_rel);
    TEST_CHECK_MSG(part0 != fake.stats.calls.end(), "the pack part tail was never requested");
    const auto unifont = fake.stats.calls.find(prod.loose[2].blob_rel);
    TEST_CHECK_MSG(unifont != fake.stats.calls.end(), "the T1 loose tail was never requested");

    // the install is byte-correct
    std::vector<uint8_t> bytes;
    TEST_CHECK(test_read_file(fs_path_join(params.install_dir, "tiledata.mul"), bytes));
    TEST_CHECK(bytes == prod.loose[0].payload);
    CUopMappedFile pack;
    uo_data_init(fs_path_ascii(params.install_dir), 0, false);
    TEST_CHECK(g_FileManager.UopLoadFile(pack, "MainMisc.uop", false));
    TEST_CHECK(pack.GetData(pack.GetAsset(prod.parts[0].uop_hash())) == prod.parts[0].payload);

    vfs_shutdown();
    // restore: the default transport takes over again
    TEST_CHECK(vfs_transport_set(previous) == &fake);
    TEST_CHECK(vfs_transport_active() == vfs_transport_default());
}

VFS_TEST(transport_range_unsupported_by_default)
{
    uint8_t sink_bucket[8] = {};
    // the default transport answers range gets with "unsupported": callers
    // keep their whole-fetch fallback (the seam contract)
    const vfs_transport_result result = vfs_transport_default()->range(
        "http://unused.test/blob", "base/blob", 0, 8, nullptr, sink_bucket, 0, false);
    TEST_CHECK(!result.ok);
    TEST_CHECK(result.status == 0);
}

// ---------------------------------------------------------------------------
// fixture server hardening: COOP/COEP headers
// ---------------------------------------------------------------------------

namespace
{
// libcurl header sink (plain function: setopt is variadic, lambdas do
// not decay there)
size_t test_header_sink(const char *buffer, size_t size, size_t nitems, void *outstr)
{
    static_cast<std::string *>(outstr)->append(buffer, size * nitems);
    return size * nitems;
}

// GETs a URL and returns the raw response headers (libcurl, header dump)
std::string test_http_headers(const std::string &url)
{
    CURL *curl = curl_easy_init();
    if (curl == nullptr)
    {
        return {};
    }
    std::string headers;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD is enough
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, test_header_sink);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return headers;
}

bool test_headers_contain(const std::string &headers, const char *line)
{
    return headers.find(line) != std::string::npos;
}
} // namespace

VFS_TEST(transport_server_coop_coep_headers)
{
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "coop_coep"));

    // with --coop-coep the isolation headers are on every response
    test_server isolated;
    TEST_CHECK(test_server_start_ex(isolated, prod.root, false, "--coop-coep"));
    const auto headers = test_http_headers(test_server_url(isolated, "/hits"));
    TEST_CHECK_MSG(headers.size() > 0, "no response from the fixture server");
    TEST_CHECK_MSG(
        test_headers_contain(headers, "Cross-Origin-Opener-Policy: same-origin"),
        "COOP header missing");
    TEST_CHECK_MSG(
        test_headers_contain(headers, "Cross-Origin-Embedder-Policy: require-corp"),
        "COEP header missing");
    test_server_stop(isolated);

    // the default stays plain (existing CORS-only tests unaffected)
    test_server plain;
    TEST_CHECK(test_server_start(plain, prod.root, false));
    const auto plain_headers = test_http_headers(test_server_url(plain, "/hits"));
    TEST_CHECK(!test_headers_contain(plain_headers, "Cross-Origin-Opener-Policy"));
    TEST_CHECK(!test_headers_contain(plain_headers, "Cross-Origin-Embedder-Policy"));
    test_server_stop(plain);
}

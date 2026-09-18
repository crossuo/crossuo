// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// fetch tests: the streaming boot pipeline against the local fixture
// server (tests/vfs/http_server.py, ephemeral port, no external network):
//   - plan + fetch + materialize over HTTP, verified through the production
//     UopLoadFile/GetData path and byte-compare of the loose files
//   - hit-counter proves a second boot performs zero new requests
//   - 404: a missing T0 blob fails the boot naming the file, a missing T1
//     blob is logged and skipped (retried on the next boot)
//   - offline mode never issues a request and fails cleanly on an empty store
//   - in-flight dedup: two waiters on the same tail produce one GET
//   - the asset recorder writes a valid CSV (explicit and default scenario)
//   - a body far larger than the expected size aborts at the transfer cap

#define LOGGER_MODULE TestVfs

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <atomic>
#include <chrono>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "test_main.h"
#include "test_product.h"
#include "test_server.h"

#include <external/process.h>
#include <xuocore/common.h>
#include <common/logging/logging.h>
#include <xuocore/http.h>
#include <xuocore/uodata.h>
#include <xuocore/uop.h>
#include <xuocore/vfs.h>
#include <xuocore/vfs_fetch.h>
#include <xuocore/vfs_source_aops.h>

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static bool test_read_file(const fs_path &path, std::vector<uint8_t> &out)
{
    FILE *fp = fopen(fs_path_ascii(path), "rb");
    if (!fp)
        return false;
    fseek(fp, 0, SEEK_END);
    const long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    out.resize(size_t(size));
    const size_t read = fread(out.data(), 1, out.size(), fp);
    fclose(fp);
    return read == out.size();
}

// streaming params pointed at a fixture product over HTTP: the fetch store
// (<root>/fetch-blobs) starts empty, the server serves the product tree
static vfs_params test_fetch_params(const test_product_data &prod, const test_server &srv)
{
    vfs_params params;
    params.enabled = true;
    params.install_dir = fs_path_join(prod.root, "fetch-install");
    params.blobs_dir = fs_path_join(prod.root, "fetch-blobs");
    params.product_url = test_server_url(srv, "/prod.xml");
    return params;
}

static astr_t test_loose_tail(const char *name)
{
    uint32_t ph = 0, sh = 0;
    vfs_name_hash(name, ph, sh);
    char buf[128];
    snprintf(buf, sizeof(buf), "base/unpacked/%08x%08x", ph, sh);
    return astr_t(buf);
}

// loads a materialized pack through the exact production boot path
static bool test_load_pack(const vfs_params &params, const char *name, CUopMappedFile &file)
{
    uo_data_init(fs_path_ascii(params.install_dir), 0, false);
    fetch_log_mute mute;
    return g_FileManager.UopLoadFile(file, name, false);
}

// ---------------------------------------------------------------------------
// tests
// ---------------------------------------------------------------------------

// keeps the module state clean even when an assertion returns early
// (test_vfs_shutdown lives in test_server.h)

// the full pipeline: plan over HTTP, T0 fetch + materialize, T1 background,
// production pack parsing; then a request-free second boot
VFS_TEST(fetch_http_end_to_end)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    prod.pack_name = "MainMisc.uop";
    // the first part becomes the Asset::Strings hash => a T0 (MainMisc core)
    // asset, so the blocking tier fetches + materializes one real block
    prod.first_part_ph = 0x9612dc85;
    prod.first_part_sh = 0x00e53aa2;
    prod.extra_valid_loose.push_back({ "unifont1.mul", 1, 1024 }); // T1 static set
    TEST_CHECK(test_build_product(prod, "fetch_e2e"));

    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));

    const vfs_params params = test_fetch_params(prod, srv);
    vfs_set_params(params);

    TEST_CHECK(vfs_enabled());
    TEST_CHECK_MSG(vfs_boot(nullptr), "boot over http failed"); // plan + T0 + start T1
    vfs_boot_wait();                                            // headless: also finish T1

    // T0 loose file byte-correct; T1 fetched by the background tier; T2/T3
    // converged by the continuous sweep (boot_wait waits for it)
    const auto tiledata = fs_path_join(params.install_dir, "tiledata.mul");
    const auto unifont = fs_path_join(params.install_dir, "unifont1.mul");
    const auto music = fs_path_join(params.install_dir, "Music", "Digital", "Britainpos.mp3");

    std::vector<uint8_t> bytes;
    TEST_CHECK_MSG(test_read_file(tiledata, bytes), "tiledata.mul not materialized");
    TEST_CHECK_MSG(bytes == prod.loose[0].payload, "tiledata.mul bytes differ");
    TEST_CHECK_MSG(test_read_file(unifont, bytes), "unifont1.mul not materialized");
    TEST_CHECK_MSG(bytes == prod.loose[2].payload, "unifont1.mul bytes differ");
    // T3 (music) was deferred at boot, then the continuous sweep fetched it
    // behind the game: by boot_wait the file materializes with real bytes
    std::vector<uint8_t> music_bytes;
    TEST_CHECK_MSG(test_read_file(music, music_bytes), "music file missing");
    TEST_CHECK_MSG(
        music_bytes == prod.loose[1].payload,
        "the sweep must have fetched + materialized T3 by boot_wait");

    // the pack was skeleton-prepared, parses through the production path and
    // its T0 (Strings) block decodes to the exact payload
    CUopMappedFile pack;
    TEST_CHECK_MSG(test_load_pack(params, "MainMisc.uop", pack), "UopLoadFile failed");
    const auto &strings_part = prod.parts[0];
    const auto *entry = pack.GetAsset(strings_part.uop_hash());
    TEST_CHECK_MSG(entry != nullptr, "the T0 block is not materialized");
    TEST_CHECK(pack.GetData(entry) == strings_part.payload);
    // trap: the T2 block does not share a page with the entry table under
    // the stored layout (blocks space by the decompressed payload), so the
    // table walk does not reach it; touching its planned bytes faults, the
    // handler streams the blob from the still-running fixture server and
    // patches the entry - exactly the on-demand streaming the trap provides
    const auto &streamed_part = prod.parts[1];
    const uint64_t table_end =
        sizeof(UopHeader) + sizeof(UopSection) + sizeof(UopFileEntry) * prod.parts.size();
    const uint64_t streamed_at = table_end + prod.parts[0].ml + prod.parts[0].payload.size() +
                                 streamed_part.ml;
    volatile uint8_t probe = pack.Start[streamed_at];
    (void)probe;

    CUopMappedFile reloaded;
    TEST_CHECK_MSG(test_load_pack(params, "MainMisc.uop", reloaded), "pack reload failed");
    const auto *streamed = reloaded.GetAsset(streamed_part.uop_hash());
    TEST_CHECK_MSG(streamed != nullptr, "the T2 block was not streamed on access");
    TEST_CHECK(reloaded.GetData(streamed) == streamed_part.payload);
    reloaded.Unload();

    vfs_shutdown();
    test_server_stop(srv);
}

// downloads are whole-blob GETs (pack parts and loose files alike), so a
// server without Range support behaves identically; documented contract for
// the range-streaming fallback (whole-file GET + per-range stub fill)
VFS_TEST(fetch_http_no_range_whole_gets)
{
    test_product_data prod;
    prod.pack_name = "MainMisc.uop";
    prod.first_part_ph = 0x9612dc85;
    prod.first_part_sh = 0x00e53aa2;
    TEST_CHECK(test_build_product(prod, "fetch_norange"));

    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, true /*no range*/));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));

    fetch_log_mute mute;
    vfs_set_params(test_fetch_params(prod, srv));
    TEST_CHECK_MSG(vfs_boot(nullptr), "boot against a no-range server failed");
    vfs_boot_wait();
    vfs_shutdown();

    std::vector<uint8_t> bytes;
    TEST_CHECK(test_read_file(fs_path_join(vfs_get_params().install_dir, "tiledata.mul"), bytes));
    TEST_CHECK(bytes == prod.loose[0].payload);
    test_server_stop(srv);
}

VFS_TEST(fetch_second_boot_zero_requests)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    prod.pack_name = "MainMisc.uop";
    prod.extra_valid_loose.push_back({ "unifont1.mul", 1, 1024 });
    TEST_CHECK(test_build_product(prod, "fetch_hits"));

    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));

    vfs_set_params(test_fetch_params(prod, srv));
    TEST_CHECK(vfs_boot(nullptr));
    vfs_boot_wait();
    vfs_shutdown();

    const uint64_t after_first = test_server_total_hits(srv);
    TEST_CHECK(after_first > 0);

    // second boot: the store is complete and the serial matches, so the
    // pipeline itself is request-free. The product refresh is the
    // one allowed exception: an online boot re-fetches the ~1.6 KB product
    // manifest to detect serial drift (and nothing else may be requested)
    TEST_CHECK(vfs_boot(nullptr));
    vfs_boot_wait();
    vfs_shutdown();
    const uint64_t after_second = test_server_total_hits(srv);
    TEST_CHECK_MSG(
        after_second - after_first == 1,
        "second boot issued %" PRIu64 " extra requests (only the product refresh is allowed)",
        after_second - after_first);
    TEST_CHECK_MSG(
        test_server_hits(srv, "/prod.xml") == 2,
        "the product manifest must be the only second-boot request");

    // and the install still verifies without corruption (only T2/T3 missing)
    vfs_verify_result result;
    const int corrupt = vfs_verify(&result);
    TEST_CHECK_MSG(corrupt == 0, "verify reported %d corrupt assets", corrupt);
    test_server_stop(srv);
}

VFS_TEST(fetch_t0_missing_fails_boot)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "fetch_t0_404"));

    // the boot-critical tiledata blob 404s
    const astr_t tiledata_rel = test_loose_tail("tiledata.mul");
    test_product_remove_blob(prod, tiledata_rel);

    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));

    fetch_log_mute mute;
    vfs_set_params(test_fetch_params(prod, srv));
    TEST_CHECK_MSG(!vfs_boot(nullptr), "boot must fail when a T0 blob is missing");
    vfs_shutdown();

    // the server really answered the request for it
    TEST_CHECK(test_server_hits(srv, ("/blobs/" + tiledata_rel).c_str()) > 0);
    test_server_stop(srv);
}

VFS_TEST(fetch_t1_missing_is_logged_and_skipped)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    prod.extra_valid_loose.push_back({ "unifont1.mul", 1, 1024 }); // T1
    TEST_CHECK(test_build_product(prod, "fetch_t1_404"));

    const astr_t tail = test_loose_tail("unifont1.mul");
    test_product_remove_blob(prod, tail);

    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));

    fetch_log_mute mute;
    vfs_set_params(test_fetch_params(prod, srv));
    TEST_CHECK_MSG(vfs_boot(nullptr), "a missing T1 blob must not fail the boot");
    vfs_boot_wait();
    vfs_shutdown();

    // the skeleton still sized the file (Load() finds a valid, sparse,
    // zero-filled file); the asset simply stays pending for the next boot
    std::vector<uint8_t> bytes;
    TEST_CHECK_MSG(
        test_read_file(fs_path_join(vfs_get_params().install_dir, "unifont1.mul"), bytes),
        "unifont1.mul skeleton missing");
    bool zeroed = bytes.size() == 1024;
    for (uint8_t b : bytes)
        zeroed = zeroed && b == 0;
    TEST_CHECK_MSG(zeroed, "unifont1.mul must stay unfetched (zero-filled)");
    vfs_verify_result result;
    TEST_CHECK(vfs_verify(&result) == 0);                               // sized files verify clean
    TEST_CHECK(test_server_hits(srv, ("/blobs/" + tail).c_str()) == 1); // the 404
    test_server_stop(srv);
}

VFS_TEST(fetch_offline_empty_store_fails_cleanly)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "fetch_offline"));

    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));

    fetch_log_mute mute;
    vfs_params params = test_fetch_params(prod, srv);
    params.offline = true; // never hit the network; the store is empty
    vfs_set_params(params);
    TEST_CHECK_MSG(!vfs_boot(nullptr), "offline boot with an empty store must fail");
    vfs_shutdown();
    TEST_CHECK_MSG(
        test_server_total_hits(srv) == 0,
        "offline mode issued requests (%" PRIu64 ")",
        test_server_total_hits(srv));
    test_server_stop(srv);
}

VFS_TEST(fetch_offline_store_only)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "fetch_offline_ok"));

    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));

    fetch_log_mute mute;
    vfs_params params = test_fetch_params(prod, srv);
    params.offline = true;
    // a complete local store: boot materializes without a single request
    params.blobs_dir = prod.blobs;
    params.product_file = fs_path_join(prod.root, "prod.xml");
    vfs_set_params(params);
    TEST_CHECK_MSG(vfs_boot(nullptr), "offline boot with a full store must succeed");
    vfs_boot_wait();
    vfs_shutdown();

    std::vector<uint8_t> bytes;
    TEST_CHECK(test_read_file(fs_path_join(params.install_dir, "tiledata.mul"), bytes));
    TEST_CHECK(bytes == prod.loose[0].payload);
    TEST_CHECK_MSG(
        test_server_total_hits(srv) == 0,
        "offline mode issued requests (%" PRIu64 ")",
        test_server_total_hits(srv));
    test_server_stop(srv);
}

VFS_TEST(fetch_dedup_same_tail)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "fetch_dedup"));

    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));

    fetch_log_mute mute;
    // direct engine use: this test owns its curl globals (no boot ran yet)
    http_init();
    // one tail, two racing waiters
    vfs_fetch_params fetch_params;
    fetch_params.workers = 2;
    TEST_CHECK(vfs_fetch_start(fetch_params));
    vfs_fetch_set_store(fs_path_join(prod.root, "fetch-blobs"));
    const auto repo = test_server_url(srv, "/blobs/");
    vfs_fetch_set_repos(repo.c_str(), repo.c_str());

    const astr_t tail = test_loose_tail("tiledata.mul");

    std::atomic<bool> ok_a{ false }, ok_b{ false };
    std::thread ta([&] { ok_a = vfs_fetch_blob_to_store(tail.c_str(), 2048, false); });
    std::thread tb([&] { ok_b = vfs_fetch_blob_to_store(tail.c_str(), 2048, false); });
    ta.join();
    tb.join();
    TEST_CHECK_MSG(ok_a && ok_b, "both waiters must succeed");
    vfs_fetch_stop();

    TEST_CHECK_MSG(
        test_server_hits(srv, ("/blobs/" + tail).c_str()) == 1,
        "dedup failed: more than one GET for the same tail");
    test_server_stop(srv);
}

VFS_TEST(fetch_body_cap_aborts_oversized)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "fetch_bodycap"));

    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));

    fetch_log_mute mute;
    http_init(); // engine-level use: this test owns its curl globals
    vfs_fetch_params fetch_params;
    fetch_params.workers = 1;
    TEST_CHECK(vfs_fetch_start(fetch_params));
    vfs_fetch_set_store(fs_path_join(prod.root, "fetch-blobs"));
    const auto repo = test_server_url(srv, "/blobs/");
    vfs_fetch_set_repos(repo.c_str(), repo.c_str());

    // the served body is exactly ONE byte over the expected size: the exact
    // body cap must abort the transfer mid-flight - the legacy 64 KB slack of
    // the unseamed engine would have let this body complete, failing only at
    // the engine's post-transfer size check
    const astr_t tail = test_loose_tail("tiledata.mul");
    std::vector<uint8_t> big(2048 + 1, 0xAB);
    TEST_CHECK(test_write_file(fs_path_join(prod.blobs, tail), big));
    TEST_CHECK_MSG(
        !vfs_fetch_blob_to_store(tail.c_str(), 2048, false),
        "an oversized body must fail the fetch");
    TEST_CHECK_MSG(
        !fs_path_exists(fs_path_join(prod.root, "fetch-blobs", tail)),
        "an aborted body must not land in the store");
    vfs_fetch_stop();
    test_server_stop(srv);
}

VFS_TEST(fetch_recorder_writes_csv)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "fetch_record"));

    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));

    fetch_log_mute mute;
    vfs_params params = test_fetch_params(prod, srv);
    const auto csv = fs_path_join(prod.root, "record.csv");
    params.record_path = csv;
    params.record_scenario = "fixture-run"; // explicit scenario
    vfs_set_params(params);
    TEST_CHECK(vfs_boot(nullptr)); // boot fetches are cache-warming: not recorded
    vfs_boot_wait();
    // game demand: resolving an asset through the store records it
    VfsPlan plan;
    TEST_CHECK(vfs_plan_ensure(plan));
    size_t tiledata_index = SIZE_MAX;
    for (size_t i = 0; i < plan.files.size(); ++i)
    {
        if (!plan.files[i].is_uop && plan.files[i].name == "tiledata.mul")
        {
            tiledata_index = i;
            break;
        }
    }
    TEST_CHECK_MSG(tiledata_index != SIZE_MAX, "tiledata.mul not in the plan");
    std::vector<uint8_t> states;
    vfs_store_params store_params;
    store_params.blobs_dir = params.blobs_dir;
    store_params.install_dir = params.install_dir;
    TEST_CHECK_MSG(
        vfs_store_rematerialize_asset(plan, store_params, states, nullptr, uint32_t(tiledata_index), 0),
        "demand resolution failed");
    vfs_shutdown();

    std::vector<uint8_t> bytes;
    TEST_CHECK_MSG(test_read_file(csv, bytes), "record csv missing");
    const astr_t text(bytes.begin(), bytes.end());
    TEST_CHECK_MSG(
        text.rfind("hash,pack,ul,tier_hint,first_access_ms,scenario\n", 0) == 0,
        "unexpected csv header");
    // the T0 loose file was materialized this run: record with T0 hint
    TEST_CHECK_MSG(text.find(",tiledata.mul,") != astr_t::npos, "tiledata record missing");
    TEST_CHECK_MSG(text.find("fixture-run\n") != astr_t::npos, "scenario column missing");

    // no scenario configured: the column defaults to "default"; a fresh
    // store+install reruns the full flow so a transition is recorded again
    vfs_params again = test_fetch_params(prod, srv);
    again.install_dir = fs_path_join(prod.root, "fetch-install2");
    again.blobs_dir = fs_path_join(prod.root, "fetch-blobs2");
    const auto csv2 = fs_path_join(prod.root, "record-default.csv");
    again.record_path = csv2;
    vfs_set_params(again);
    TEST_CHECK(vfs_boot(nullptr));
    vfs_boot_wait();
    TEST_CHECK(vfs_plan_ensure(plan));
    std::vector<uint8_t> states2;
    TEST_CHECK_MSG(
        vfs_store_rematerialize_asset(plan, store_params, states2, nullptr, uint32_t(tiledata_index), 0),
        "second demand resolution failed");
    vfs_shutdown();
    TEST_CHECK(test_read_file(csv2, bytes));
    const astr_t text2(bytes.begin(), bytes.end());
    TEST_CHECK_MSG(text2.find("default\n") != astr_t::npos, "default scenario missing");
    test_server_stop(srv);
}

// ---------------------------------------------------------------------------
// the continuous background sweep (T2/T3) and the in-game pool demotion
// ---------------------------------------------------------------------------

static uint64_t test_now_ms()
{
    return uint64_t(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

// the sweep: a plain online boot converges the whole plan behind the game -
// on-demand tiers included, no game-side demand and no fetch_all involved
VFS_TEST(fetch_sweep_converges_ondemand_tiers)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "fetch_sweep"));
    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));

    const auto params = test_fetch_params(prod, srv); // background_sweep defaults on
    vfs_set_params(params);
    TEST_CHECK_MSG(vfs_boot(nullptr), "boot failed");
    vfs_boot_wait(); // T1 + convergence + the sweep
    vfs_shutdown();

    // every pack asset with content (T2 by default; the fixture's zero-byte
    // part is the planned hole and stays one) materialized with real bytes
    CUopMappedFile pack;
    TEST_CHECK_MSG(test_load_pack(params, prod.pack_name.c_str(), pack), "pack load failed");
    for (const auto &part : prod.parts)
    {
        if (part.payload.empty())
            continue; // the planned hole
        const auto *entry = pack.GetAsset(part.uop_hash());
        TEST_CHECK_MSG(entry != nullptr, "a pack block is missing after the sweep");
        if (entry != nullptr)
            TEST_CHECK(pack.GetData(entry) == part.payload);
    }
    // the T3 music file (deferred at boot) converged too
    const auto music = fs_path_join(params.install_dir, "Music", "Digital", "Britainpos.mp3");
    std::vector<uint8_t> bytes;
    TEST_CHECK_MSG(test_read_file(music, bytes), "T3 music not materialized");
    TEST_CHECK_MSG(bytes == prod.loose[1].payload, "T3 music bytes differ");
    test_server_stop(srv);
}

// --streaming-no-sweep (params.background_sweep = false) restores the old
// on-demand-only contract: the on-demand tiers stay untouched after boot
VFS_TEST(fetch_no_sweep_keeps_ondemand_untouched)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "fetch_nosweep"));
    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));

    auto params = test_fetch_params(prod, srv);
    params.background_sweep = false;
    vfs_set_params(params);
    TEST_CHECK_MSG(vfs_boot(nullptr), "boot failed");
    vfs_boot_wait();
    vfs_shutdown();

    // the T2 pack blob and the T3 music blob were never requested
    TEST_CHECK_MSG(
        !fs_path_exists(fs_path_join(params.blobs_dir, prod.parts[1].blob_rel)),
        "the sweep-off boot must not fetch T2 pack blobs");
    TEST_CHECK_MSG(
        !fs_path_exists(fs_path_join(params.blobs_dir, prod.loose[1].blob_rel)),
        "the sweep-off boot must not fetch T3 blobs");
    test_server_stop(srv);
}

// the in-game demotion parks one worker (N -> N-1 active, clamped at 1)
VFS_TEST(fetch_ingame_parks_one_worker)
{
    test_vfs_shutdown guard;
    vfs_fetch_params params;
    params.workers = 4;
    TEST_CHECK(vfs_fetch_start(params));
    TEST_CHECK_MSG(vfs_fetch_active_workers() == 4, "the pool starts at full size");
    vfs_fetch_set_ingame();
    TEST_CHECK_MSG(vfs_fetch_active_workers() == 3, "in-game parks exactly one worker");
    vfs_fetch_stop();

    // single-worker pools clamp: the last worker never parks
    params.workers = 1;
    TEST_CHECK(vfs_fetch_start(params));
    vfs_fetch_set_ingame();
    TEST_CHECK_MSG(vfs_fetch_active_workers() == 1, "the last worker must stay active");
    vfs_fetch_stop();
}

// the sweep lane: with 2 workers the sweep runs one job at a time (cap =
// active-1), so a demand request enqueued mid-sweep always jumps the queue -
// it completes before any not-yet-started sweep job (the lane blocks the
// second worker from taking sweep work, and the stalled S1 holds the lane)
VFS_TEST(fetch_demand_beats_sweep)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "fetch_lane"));
    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));

    vfs_fetch_params fparams;
    fparams.workers = 2;
    http_init();
    TEST_CHECK(vfs_fetch_start(fparams));
    vfs_fetch_set_store(fs_path_join(prod.root, "lane-blobs"));
    const astr_t repo = test_server_url(srv, "/blobs/");
    vfs_fetch_set_repos(repo.c_str(), repo.c_str());

    // three sweep blobs saturate the lane; the demand blob races them. S1
    // rides the server's /slow/ stall so the first sweep stays in flight
    // while demand is enqueued (deterministic ordering). Threads only
    // record outcomes - the test framework is not thread-safe
    const astr_t slow_rel = astr_t("slow/") + prod.parts[0].blob_rel;
    std::atomic<uint64_t> done_at[4] = {{0}, {0}, {0}, {0}};
    std::atomic<bool> ok[4] = {{false}, {false}, {false}, {false}};
    const auto sweep_job = [&prod, &done_at, &ok](const astr_t &rel, size_t slot) {
        ok[slot] = vfs_fetch_blob_to_store(
            rel.c_str(), prod.parts[slot].blob.size(), false, 0, VFS_FETCH_PRIO_SWEEP);
        done_at[slot] = test_now_ms();
    };
    const auto demand_job = [&prod, &done_at, &ok]() {
        const auto &tile = prod.loose[0];
        ok[3] = vfs_fetch_blob_to_store(
            tile.blob_rel.c_str(), tile.blob.size(), false, 0, VFS_FETCH_PRIO_DEMAND);
        done_at[3] = test_now_ms();
    };
    std::thread s0(sweep_job, slow_rel, 0), s1(sweep_job, prod.parts[1].blob_rel, 1),
        s2(sweep_job, prod.parts[2].blob_rel, 2);
    for (int i = 0; i < 5000 && vfs_fetch_sweep_running() == 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    TEST_CHECK_MSG(vfs_fetch_sweep_running() == 1, "the lane must cap sweeps below the pool");

    std::thread d(demand_job);
    d.join();
    s0.join();
    s1.join();
    s2.join();
    vfs_fetch_stop();
    for (int i = 0; i < 4; ++i)
        TEST_CHECK_MSG(ok[i], "every lane test fetch must succeed");
    TEST_CHECK_MSG(
        done_at[3] <= done_at[1] && done_at[3] <= done_at[2],
        "demand must complete before not-yet-started sweep jobs");
    test_server_stop(srv);
}

// dedup promotion: the game demanding a tail that the sweep already has
// QUEUED (lane full, not started) must jump the queue - the demand call
// promotes the queued job and completes while the sweep still holds the
// lane on the stalled first blob. Without promotion this demand waits
// behind the lane and lands after the stalled sweep finishes
VFS_TEST(fetch_demand_promotes_queued_sweep)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "fetch_promote"));
    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));

    vfs_fetch_params fparams;
    fparams.workers = 2;
    http_init();
    TEST_CHECK(vfs_fetch_start(fparams));
    vfs_fetch_set_store(fs_path_join(prod.root, "promote-blobs"));
    const astr_t repo = test_server_url(srv, "/blobs/");
    vfs_fetch_set_repos(repo.c_str(), repo.c_str());

    // S1 stalls in flight holding the lane; S2/S3 sit queued behind it
    const astr_t slow_rel = astr_t("slow/") + prod.parts[0].blob_rel;
    std::atomic<uint64_t> done_at[3] = {{0}, {0}, {0}};
    std::atomic<bool> ok[3] = {{false}, {false}, {false}};
    const auto sweep_job = [&prod, &done_at, &ok](const astr_t &rel, size_t slot) {
        ok[slot] = vfs_fetch_blob_to_store(
            rel.c_str(), prod.parts[slot].blob.size(), false, 0, VFS_FETCH_PRIO_SWEEP);
        done_at[slot] = test_now_ms();
    };
    std::thread s0(sweep_job, slow_rel, 0), s1(sweep_job, prod.parts[1].blob_rel, 1),
        s2(sweep_job, prod.parts[2].blob_rel, 2);
    for (int i = 0; i < 5000 && vfs_fetch_sweep_running() == 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    TEST_CHECK_MSG(vfs_fetch_sweep_running() == 1, "the lane must cap sweeps below the pool");

    // demand the QUEUED S2's tail: the dedup must promote it to the demand
    // lane so it fetches now, on the idle second worker, while S1 stalls
    TEST_CHECK(vfs_fetch_blob_to_store(
        prod.parts[1].blob_rel.c_str(), prod.parts[1].blob.size(), false, 0,
        VFS_FETCH_PRIO_DEMAND));

    s0.join();
    s1.join();
    s2.join();
    vfs_fetch_stop();
    for (int i = 0; i < 3; ++i)
        TEST_CHECK_MSG(ok[i], "every promotion test fetch must succeed");
    TEST_CHECK_MSG(
        done_at[1] < done_at[0],
        "the promoted tail must complete while the stalled sweep holds the lane");
    test_server_stop(srv);
}

// shutdown mid-streaming: vfs_shutdown must SIGNAL the background tier.
// Pre-fix the flag was never set: quitting before the first frame hung
// forever on the tier's frame gate, and quitting mid-sweep walked every
// remaining chunk against the dead pool inside the join (the materialize
// + "pool not running" spam)
VFS_TEST(fetch_shutdown_stops_the_background_tier)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "fetch_shutdown"));
    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));

    const auto params = test_fetch_params(prod, srv);
    vfs_set_params(params);
    TEST_CHECK_MSG(vfs_boot(nullptr), "boot failed");
    // deliberately NO first-frame notification: the background tier drains
    // its (empty) T1 list and parks on the frame gate
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const auto start = test_now_ms();
    vfs_shutdown();
    const auto elapsed = test_now_ms() - start;
    TEST_CHECK_MSG(
        elapsed < 10000,
        "shutdown must signal the background tier (took %" PRIu64 "ms)",
        elapsed);
    test_server_stop(srv);
}

// quitting mid-sweep (the live case): shutdown aborts the sweep promptly
// and the interrupted install stays bootable - the journal replays the
// marks that landed, the next boot re-sweeps the rest
VFS_TEST(fetch_shutdown_mid_sweep_returns_promptly)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "fetch_shutdown_sweep"));
    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));

    const auto params = test_fetch_params(prod, srv);
    vfs_set_params(params);
    TEST_CHECK_MSG(vfs_boot(nullptr), "boot failed");
    vfs_notify_first_frame(); // the sweep starts behind the game
    // wait until the sweep made progress: a T2 blob landed in the store
    const auto t2_blob = fs_path_join(params.blobs_dir, prod.parts[1].blob_rel);
    for (int i = 0; i < 5000 && !fs_path_exists(t2_blob); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    TEST_CHECK_MSG(fs_path_exists(t2_blob), "the sweep never started");

    const auto start = test_now_ms();
    vfs_shutdown();
    const auto elapsed = test_now_ms() - start;
    TEST_CHECK_MSG(
        elapsed < 10000,
        "shutdown must abort the sweep promptly (took %" PRIu64 "ms)",
        elapsed);

    // the interrupted install must remain bootable
    vfs_set_params(params);
    TEST_CHECK_MSG(vfs_boot(nullptr), "reboot after a mid-sweep shutdown failed");
    vfs_boot_wait();
    vfs_shutdown();
    test_server_stop(srv);
}

// ---------------------------------------------------------------------------
// the aops patch format: patchcfg + UTF-16 patchlist -> the same vfs plan
// ---------------------------------------------------------------------------

// writes the patchlist as UTF-16LE with BOM and CRLF (the wire format)
static void test_aops_write_patchlist(const fs_path &path, const std::vector<astr_t> &lines)
{
    astr_t utf16;
    utf16.push_back(char(0xFF));
    utf16.push_back(char(0xFE));
    for (const auto &line : lines)
    {
        for (const char c : line)
        {
            utf16.push_back(c);
            utf16.push_back(char(0));
        }
        utf16.push_back('\r');
        utf16.push_back(char(0));
        utf16.push_back('\n');
        utf16.push_back(char(0));
    }
    std::vector<uint8_t> payload(utf16.begin(), utf16.end());
    fs_file_write(path, payload);
}

// the offline plan build: the patchlist filters down to UO data files,
// lowercased, deduped; launcher/web files stay out
VFS_TEST(aops_plan_filters_and_normalizes)
{
    test_vfs_shutdown guard;
    const auto root = test_dir("aops_plan");
    const auto patchlist = fs_path_join(root, "patchlist");
    const auto blobs = fs_path_join(root, "blobs");
    TEST_CHECK(fs_path_create(fs_path_join(blobs, "aops")));
    vfs_aops_config cfg;
    TEST_CHECK(vfs_aops_parse_patchcfg(
        reinterpret_cast<const uint8_t *>("[project]\ncustom_extension=aops\n"
                                         "[patch_system_configuration]\n"
                                         "online_patch_directory=/data/\n"),
        strlen("[project]\ncustom_extension=aops\n"
               "[patch_system_configuration]\n"
               "online_patch_directory=/data/\n"),
        "https://shard.test/fc", &cfg));
    TEST_CHECK_MSG(
        cfg.patch_directory == "https://shard.test/fc/data/",
        "the data url must resolve against the root");
    TEST_CHECK_MSG(cfg.custom_extension == "aops", "the extension must carry");

    test_aops_write_patchlist(patchlist, {
        "Faerham Citadel\\tiledata.mul|01012024|12|0|0|0|3072|No|No|aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "Faerham Citadel\\Cliloc.enu|02022024|1|2|3|0|654|No|No|bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        "data\\FCLauncher.exe|03032025|1|2|3|0|123|Yes|No|cccccccccccccccccccccccccccccccc",
        "Faerham Citadel\\shard art.png|04042025|1|2|3|0|999|No|No|dddddddddddddddddddddddddddddddd",
        "Faerham Citadel\\TILEDATA.MUL|05052025|1|2|3|0|111|No|No|eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee",
    });

    vfs_params params;
    params.enabled = true;
    params.install_dir = fs_path_join(root, "install");
    params.blobs_dir = blobs;
    params.product_file = patchlist;
    vfs_set_params(params);

    VfsPlan plan;
    TEST_CHECK_MSG(vfs_plan_rebuild(plan), "the aops plan build failed");
    TEST_CHECK_MSG(plan.files.size() == 2, "only the UO data files may plan");
    bool have_tile = false, have_cliloc = false;
    for (const auto &file : plan.files)
    {
        if (file.name == "tiledata.mul")
        {
            have_tile = true;
            TEST_CHECK_MSG(file.size == 3072, "the first entry wins the dedup");
        }
        else if (file.name == "cliloc.enu")
        {
            have_cliloc = true;
        }
        TEST_CHECK_MSG(file.name == str_lower(file.name), "names install lowercase");
    }
    TEST_CHECK_MSG(have_tile && have_cliloc, "the UO data entries must plan");
    TEST_CHECK_MSG(
        plan.files[0].assets.front().blob == "Faerham%20Citadel/tiledata.mulaops",
        "the tail carries the url-encoded path and the aops extension");
}

// the full pipeline over http: patchcfg + patchlist fetch, T0 blocking
// install of the core file, fetch_all converging the rest - and the
// update flow (serial drift re-fetches exactly the changed file)
VFS_TEST(aops_boot_installs_and_drifts)
{
    test_vfs_shutdown guard;
    const auto root = test_dir("aops_boot");
    const auto blobs = fs_path_join(root, "data");
    test_server srv;
    TEST_CHECK(test_server_start(srv, root, false));

    const astr_t repo = test_server_url(srv, "/data/");
    const auto patchlist = fs_path_join(root, "patchlist");
    const auto patchcfg = fs_path_join(root, "patchcfg");
    const auto tile_blob = fs_path_join(blobs, "Faerham%20Citadel", "tiledata.mulaops");
    const auto cliloc_blob = fs_path_join(blobs, "Faerham%20Citadel", "cliloc.enuaops");

    const std::vector<uint8_t> tile_payload(3072, 0xAB);
    std::vector<uint8_t> cliloc_payload(654, 0xCD);
    const std::vector<uint8_t> cliloc_payload_v2(700, 0xEF);

    const auto write_product = [&](uint64_t cliloc_size) {
        test_aops_write_patchlist(patchlist, {
            "Faerham Citadel\\tiledata.mul|01012024|12|0|0|0|3072|No|No|aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            "Faerham Citadel\\Cliloc.enu|02022024|1|2|3|0|" + std::to_string(cliloc_size) +
                "|No|No|bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        });
    };
    TEST_CHECK(fs_path_create(fs_path_join(blobs, "Faerham%20Citadel")));
    TEST_CHECK(fs_file_write(fs_path_ascii(tile_blob), tile_payload));
    TEST_CHECK(fs_file_write(fs_path_ascii(cliloc_blob), cliloc_payload));
    write_product(654);
    const astr_t ini = "[project]\nname=t\n\n[patch_system_configuration]\n"
                       "online_patch_directory=" + repo + "\n";
    std::vector<uint8_t> ini_bytes(ini.begin(), ini.end());
    TEST_CHECK(fs_file_write(fs_path_ascii(patchcfg), ini_bytes));

    vfs_params params;
    params.enabled = true;
    params.install_dir = fs_path_join(root, "install");
    params.blobs_dir = fs_path_join(root, "install", "cache", "blobs");
    params.product_url = test_server_url(srv, "/patchlist");
    vfs_set_params(params);

    // boot: product + patchcfg fetch, aops plan, T0 blocking install
    TEST_CHECK_MSG(vfs_boot(nullptr), "the aops boot failed");
    std::vector<uint8_t> bytes;
    TEST_CHECK_MSG(
        test_read_file(fs_path_join(params.install_dir, "tiledata.mul"), bytes),
        "the core file must install before the first frame");
    TEST_CHECK_MSG(bytes == tile_payload, "the installed bytes must match the aops blob");
    vfs_shutdown();

    // converge the rest
    vfs_set_params(params);
    TEST_CHECK(vfs_boot(nullptr));
    TEST_CHECK(vfs_fetch_all(nullptr));
    vfs_boot_wait();
    TEST_CHECK_MSG(
        test_read_file(fs_path_join(params.install_dir, "cliloc.enu"), bytes),
        "cliloc must materialize");
    TEST_CHECK_MSG(bytes == cliloc_payload, "cliloc bytes must match");

    // the game requests mixed-case names ("Skills.idx"): the managed
    // mapping resolves them against the lowercase install
    uint8_t *mapped = nullptr;
    size_t mapped_size = 0;
    TEST_CHECK_MSG(
        vfs_map_file(fs_path_join(params.install_dir, "TileData.MUL"), mapped, mapped_size),
        "a case-variant request must map the lowercase install");
    TEST_CHECK_MSG(mapped_size == 3072, "the mapped size must match the plan");
    vfs_unmap_file(mapped, mapped_size);
    vfs_shutdown();

    // update check: same patchlist is up to date
    vfs_set_params(params);
    vfs_update_info info;
    TEST_CHECK_MSG(vfs_update_check(&info), "the aops update check failed");
    TEST_CHECK_MSG(!info.update_available, "the same patchlist is up to date");
    TEST_CHECK_MSG(
        info.remote_serial == info.local_serial, "both sides fingerprint the patchlist");

    // upstream change: the cliloc entry bumps - exactly that file re-fetches
    TEST_CHECK(fs_file_write(fs_path_ascii(cliloc_blob), cliloc_payload_v2));
    write_product(700);
    const uint64_t tile_hits =
        test_server_hits(srv, ("/data/Faerham%20Citadel/tiledata.mulaops"));
    TEST_CHECK(vfs_boot(nullptr));
    vfs_boot_wait();
    TEST_CHECK_MSG(
        test_server_hits(srv, ("/data/Faerham%20Citadel/tiledata.mulaops")) == tile_hits,
        "an unchanged file must not re-download on drift");
    TEST_CHECK_MSG(
        test_read_file(fs_path_join(params.install_dir, "cliloc.enu"), bytes) &&
            bytes == cliloc_payload_v2,
        "the changed file must update");
    vfs_shutdown();
    test_server_stop(srv);
}

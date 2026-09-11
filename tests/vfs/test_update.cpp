// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// update/diff tests: fresh product every online boot,
// serial drift re-plans and re-fetches only what changed; stub-aware verify
// and sealing; the recorder->embedded converter; the --streaming-trap=off
// escape hatch; the sparse-filesystem probe.

#include <stdio.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "test_main.h"
#include "test_product.h"
// vfs_fetch (after tinyxml2 above, before any header that pulls logging.h)
#include <xuocore/vfs_fetch.h>
#include "test_server.h"

#include <external/process.h>
#include <common/fs.h>
#include <common/str.h>
#include <xuocore/vfs.h>
#include <xuocore/vfs_state.h>
#include <xuocore/vfs_store.h>
#include <xuocore/vfs_trap.h>

#ifndef VFS_VUOI_BINARY
#define VFS_VUOI_BINARY "vuoi"
#endif

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

// this TU does not exercise every helper the shared header provides
[[maybe_unused]] static void test_product_helper_refs()
{
    (void)test_product_remove_blob;
    (void)test_build_fixture_product;
}

// re-serves the product with a bumped serial and one changed part timestamp
// (the drift the update flow must detect and diff)
static bool test_bump_product(test_product_data &prod, size_t changed_part)
{
    if (changed_part >= prod.parts.size())
        return false;
    prod.serial += 1;
    prod.parts[changed_part].timestamp += 1;
    // regenerates the manifests + prod.xml over the SAME blobs (same tails,
    // same bytes): exactly a re-signed manifest with one drifted entry
    return test_write_product(prod);
}

// runs a command capturing stdout (the converter test drives vuoi)
static bool test_run_capture(const char *const argv[], std::string &out)
{
    process_s process = {};
    if (process_create(argv, process_option_inherit_environment, &process) != 0)
        return false;
    char buf[4096];
    size_t got = 0;
    while ((got = fread(buf, 1, sizeof(buf), process_stdout(&process))) > 0)
        out.append(buf, got);
    process_destroy(&process);
    return true;
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

// total requests the running fixture server has served
static uint64_t test_hits_total(const test_server &srv)
{
    return test_server_total_hits(const_cast<test_server &>(srv));
}

static uint64_t test_hits(const test_server &srv, const char *path)
{
    return test_server_hits(const_cast<test_server &>(srv), path);
}

// index of a pack asset in the state DB (plan order)
static size_t test_global_index(const VfsPlan &plan, uint64_t uop_hash)
{
    size_t gi = 0;
    for (const auto &file : plan.files)
    {
        for (const auto &asset : file.assets)
        {
            if (asset.uop_hash == uop_hash)
                return gi;
            ++gi;
        }
    }
    return SIZE_MAX;
}

static const vfs_verify_asset *test_find_asset(const vfs_verify_result &result, uint64_t hash)
{
    for (const auto &file : result.files)
    {
        for (const auto &asset : file.assets)
        {
            if (asset.uop_hash == hash)
                return &asset;
        }
    }
    return nullptr;
}

static const vfs_verify_file *test_find_file(const vfs_verify_result &result, const char *name)
{
    for (const auto &file : result.files)
    {
        if (file.name == name)
            return &file;
    }
    return nullptr;
}

// runs a full install over the public api (the xuoi --full sequence)
static bool test_full_install(const vfs_params &params)
{
    vfs_set_params(params);
    if (!vfs_boot(nullptr))
    {
        vfs_test_fail(__FILE__, __LINE__, "full install: boot failed");
        return false;
    }
    if (!vfs_fetch_all(nullptr))
    {
        vfs_test_fail(__FILE__, __LINE__, "full install: fetch_all failed");
        return false;
    }
    vfs_boot_wait();
    vfs_materialize_stats stats;
    if (!vfs_materialize(&stats))
    {
        vfs_test_fail(__FILE__, __LINE__, "full install: materialize failed");
        return false;
    }
    vfs_verify_result result;
    const int corrupt = vfs_verify(&result);
    if (corrupt != 0)
    {
        vfs_test_fail(__FILE__, __LINE__, "full install: %d corrupt assets", corrupt);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// tests
// ---------------------------------------------------------------------------

// the update flow end to end: full cache, then a serial bump that changes one
// part's timestamp. Only that asset may re-fetch; the file unseals (its
// state resets, the pack hole re-opens) and seals again after the heal.
VFS_TEST(update_serial_bump_refetches_only_changed)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "update_single"));

    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));

    // full install: every blob is in the store, everything sealed
    TEST_CHECK_MSG(test_full_install(test_fetch_params(prod, srv)), "first install failed");
    const uint64_t part0_tail_hits = test_hits(srv, ("/blobs/" + prod.parts[0].blob_rel).c_str());
    const uint64_t part1_tail_hits = test_hits(srv, ("/blobs/" + prod.parts[1].blob_rel).c_str());
    const uint64_t tile_hits = test_hits(srv, ("/blobs/" + prod.loose[0].blob_rel).c_str());
    vfs_shutdown();

    // serial +1, part0 timestamp +1 (same blob bytes: a re-signed manifest)
    TEST_CHECK(test_bump_product(prod, 0));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));

    // the drift boot: prod + manifests refresh, the changed asset resets
    TEST_CHECK_MSG(vfs_boot(nullptr), "update boot failed");
    vfs_boot_wait();
    vfs_shutdown();

    // the changed asset is now a hole again: blob dropped, state pending -
    // verify reports it missing (NOT corrupt) and the file lost its seal
    vfs_verify_result result;
    TEST_CHECK(vfs_verify(&result) >= 0);
    const auto *part0 = test_find_asset(result, prod.parts[0].uop_hash());
    TEST_CHECK_MSG(part0 != nullptr, "changed asset missing from verify");
    TEST_CHECK_MSG(
        part0->status == vfs_verify_missing_blob,
        "changed asset must report missing-blob (got %u)",
        part0->status);
    const auto *pack = test_find_file(result, prod.pack_name.c_str());
    TEST_CHECK_MSG(pack != nullptr && !pack->sealed, "the changed file must be unsealed");
    TEST_CHECK_MSG(result.corrupted == false, "a diff reset must never read as corrupt");

    // heal through the public full-install path (boot -> fetch all tiers ->
    // materialize): exactly the changed asset re-downloads, the file re-seals
    vfs_set_params(test_fetch_params(prod, srv));
    TEST_CHECK(vfs_boot(nullptr));
    TEST_CHECK(vfs_fetch_all(nullptr));
    vfs_boot_wait();
    vfs_materialize_stats stats;
    TEST_CHECK(vfs_materialize(&stats));
    vfs_shutdown();

    const uint64_t part0_after = test_hits(srv, ("/blobs/" + prod.parts[0].blob_rel).c_str());
    const uint64_t part1_after = test_hits(srv, ("/blobs/" + prod.parts[1].blob_rel).c_str());
    const uint64_t tile_after = test_hits(srv, ("/blobs/" + prod.loose[0].blob_rel).c_str());
    TEST_CHECK_MSG(
        part0_after - part0_tail_hits == 1,
        "the changed asset must re-fetch exactly once (got %" PRIu64 ")",
        part0_after - part0_tail_hits);
    TEST_CHECK_MSG(part1_after == part1_tail_hits, "an unchanged part must not re-fetch");
    TEST_CHECK_MSG(tile_after == tile_hits, "an unchanged loose file must not re-fetch");

    result = vfs_verify_result();
    TEST_CHECK(vfs_verify(&result) >= 0);
    pack = test_find_file(result, prod.pack_name.c_str());
    TEST_CHECK_MSG(pack != nullptr && pack->sealed, "the file must re-seal after the heal");
    TEST_CHECK_MSG(result.corrupted == false, "verify must be clean after the update");
    test_server_stop(srv);
}

// a same-serial boot with a full cache: the product refresh is the only
// request (no manifests, no blobs)
VFS_TEST(update_serial_same_requests_prod_only)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "update_same"));

    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));

    TEST_CHECK_MSG(test_full_install(test_fetch_params(prod, srv)), "first install failed");
    vfs_shutdown();
    const uint64_t after_first = test_hits_total(srv);

    TEST_CHECK(vfs_boot(nullptr));
    vfs_boot_wait();
    vfs_shutdown();
    const uint64_t after_second = test_hits_total(srv);
    TEST_CHECK_MSG(
        after_second - after_first == 1,
        "a same-serial boot must only refresh the product (got %" PRIu64 " requests)",
        after_second - after_first);
    test_server_stop(srv);
}

// --full offline over a complete store: the offline boot + fetch_all succeed
// with zero requests (nothing left to fetch needs no network); the install
// stays clean through a final materialize + verify
VFS_TEST(update_full_offline_complete_store)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "update_full_offline"));

    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));

    // online full install: every blob is in the store, everything sealed
    fetch_log_mute mute;
    TEST_CHECK_MSG(test_full_install(test_fetch_params(prod, srv)), "first install failed");
    vfs_shutdown();
    const uint64_t after_online = test_hits_total(srv);

    // same dirs, offline flag: the whole --full sequence must converge
    vfs_params params = test_fetch_params(prod, srv);
    params.offline = true;
    vfs_set_params(params);
    TEST_CHECK_MSG(vfs_boot(nullptr), "offline boot failed");
    TEST_CHECK_MSG(vfs_fetch_all(nullptr), "fetch_all must succeed offline on a complete store");
    vfs_boot_wait();
    vfs_shutdown();
    const uint64_t after_offline = test_hits_total(srv);
    TEST_CHECK_MSG(
        after_offline == after_online,
        "the offline pass must issue zero requests (got %" PRIu64 ")",
        after_offline - after_online);

    vfs_materialize_stats stats;
    TEST_CHECK(vfs_materialize(&stats));
    vfs_verify_result result;
    TEST_CHECK(vfs_verify(&result) >= 0);
    TEST_CHECK_MSG(result.corrupted == false, "the offline pass must verify clean");
    test_server_stop(srv);
}

// the pure plan diff: timestamp/rh changes report per asset; identical plans
// report nothing; dropped assets are counted as removed
VFS_TEST(update_plan_diff_reports_changes)
{
    test_product_data prod;
    prod.extra_valid_loose.push_back({"unifont1.mul", 1, 1024});
    TEST_CHECK(test_build_product(prod, "update_diff_a"));

    vfs_params params;
    params.enabled = true;
    params.offline = true;
    params.install_dir = prod.install;
    params.blobs_dir = prod.blobs;
    params.product_file = fs_path_join(prod.root, "prod.xml");
    vfs_set_params(params);

    VfsPlan plan_a;
    TEST_CHECK(vfs_plan_rebuild(plan_a));
    const size_t loose_with_extra = prod.loose.size();

    // serial +1 and part0 timestamp +1: exactly one changed asset
    TEST_CHECK(test_bump_product(prod, 0));
    vfs_set_params(params);
    VfsPlan plan_b;
    TEST_CHECK(vfs_plan_rebuild(plan_b));

    vfs_plan_diff_result diff;
    TEST_CHECK(vfs_plan_diff(plan_a, plan_b, &diff));
    TEST_CHECK_MSG(diff.changed.size() == 1, "one drifted part expected");
    TEST_CHECK_MSG(diff.added.empty(), "no added assets expected");
    TEST_CHECK_MSG(diff.removed == 0, "no removed assets expected");
    TEST_CHECK_MSG(
        plan_b.files[diff.changed[0].file_index].assets[diff.changed[0].asset_index].uop_hash ==
            prod.parts[0].uop_hash(),
        "the drifted part must be the changed one");

    // identical content under a new serial: no differences
    vfs_plan_diff_result none;
    TEST_CHECK(vfs_plan_diff(plan_b, plan_b, &none));
    TEST_CHECK(none.changed.empty() && none.added.empty() && none.removed == 0);

    // the reverse comparison changes the drifted part back, removes nothing
    vfs_plan_diff_result reverse;
    TEST_CHECK(vfs_plan_diff(plan_b, plan_a, &reverse));
    TEST_CHECK_MSG(reverse.changed.size() == 1, "the drifted part must change back");
    TEST_CHECK_MSG(reverse.removed == 0, "no removals expected towards the old plan");

    // dropping the extra loose file from the product (restoring the original
    // serial + timestamp first, so the removal is the only difference): the
    // reverse diff removes it, the forward diff re-adds it
    TEST_CHECK(prod.loose.size() == loose_with_extra);
    prod.serial -= 1;
    prod.parts[0].timestamp -= 1;
    prod.loose.pop_back();
    TEST_CHECK(test_write_product(prod));
    vfs_set_params(params);
    VfsPlan plan_c;
    TEST_CHECK(vfs_plan_rebuild(plan_c));
    vfs_plan_diff_result removal;
    TEST_CHECK(vfs_plan_diff(plan_a, plan_c, &removal));
    TEST_CHECK_MSG(removal.removed == 1, "the dropped loose file must count as removed");
    TEST_CHECK_MSG(removal.changed.empty(), "no changes expected for the removal");
    vfs_plan_diff_result readd;
    TEST_CHECK(vfs_plan_diff(plan_c, plan_a, &readd));
    TEST_CHECK_MSG(readd.added.size() == 1, "the restored loose file must count as added");
}

// verify/seal stub semantics: a stubbed asset reports
// MISSING-BLOCK (labeled stubbed), never CORRUPT, and the file stays
// unsealed while a placeholder is in use; healing re-seals it
VFS_TEST(update_verify_stubbed_and_seal_gate)
{
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "update_stub"));

    vfs_params params;
    params.enabled = true;
    params.offline = true;
    params.install_dir = prod.install;
    params.blobs_dir = prod.blobs;
    params.product_file = fs_path_join(prod.root, "prod.xml");
    vfs_set_params(params);

    // complete materialize from the store: everything sealed, clean verify
    vfs_materialize_stats stats;
    TEST_CHECK(vfs_materialize(&stats));
    vfs_verify_result result;
    TEST_CHECK(vfs_verify(&result) >= 0);
    TEST_CHECK_MSG(result.corrupted == false, "a complete store must verify clean");
    const auto *pack = test_find_file(result, prod.pack_name.c_str());
    TEST_CHECK_MSG(pack != nullptr && pack->sealed, "the pack must seal when complete");

    // mark one pack asset stubbed (the state DB is the stub truth): the
    // placeholder never counts as present - or corrupt - and blocks the seal
    VfsPlan plan;
    TEST_CHECK(vfs_plan_ensure(plan));
    const size_t gi = test_global_index(plan, prod.parts[0].uop_hash());
    TEST_CHECK_MSG(gi != SIZE_MAX, "part not found in plan");
    std::vector<uint8_t> states;
    vfs_state_load(
        fs_path_join(vfs_get_params().state_dir, "state.bin"), vfs_state_asset_count(plan),
        states);
    TEST_CHECK(gi < states.size());
    states[gi] = vfs_stubbed;
    TEST_CHECK(vfs_state_save(fs_path_join(vfs_get_params().state_dir, "state.bin"), states));

    result = vfs_verify_result();
    TEST_CHECK(vfs_verify(&result) >= 0);
    const auto *part0 = test_find_asset(result, prod.parts[0].uop_hash());
    TEST_CHECK_MSG(part0 != nullptr, "stubbed asset missing from verify");
    TEST_CHECK_MSG(
        part0->status == vfs_verify_missing_block,
        "a stubbed asset must report missing-block (got %u)",
        part0->status);
    TEST_CHECK_MSG(part0->stubbed, "the stubbed flag must label the placeholder");
    TEST_CHECK_MSG(result.corrupted == false, "a stub must never read as corrupt");
    pack = test_find_file(result, prod.pack_name.c_str());
    TEST_CHECK_MSG(pack != nullptr && !pack->sealed, "a stubbed file must not be sealed");

    // the vuoi output must make the placeholder visible: the stubbed asset
    // prints the STUB label (never the bare MISSING-BLOCK bucket name)
    {
        const auto product = fs_path_join(prod.root, "prod.xml");
        std::string out;
        const char *argv[] = {
            VFS_VUOI_BINARY,
            "--verify",
            "--blobs",
            fs_path_ascii(prod.blobs),
            "--path",
            fs_path_ascii(prod.install),
            "--product",
            fs_path_ascii(product),
            nullptr
        };
        TEST_CHECK_MSG(test_run_capture(argv, out), "vuoi --verify failed to run");
        char stub_line[96];
        snprintf(
            stub_line,
            sizeof(stub_line),
            "%016llx: STUB",
            (unsigned long long)prod.parts[0].uop_hash());
        TEST_CHECK_MSG(
            out.find(stub_line) != std::string::npos,
            "the stubbed asset must print the STUB label");
        TEST_CHECK_MSG(
            out.find(": MISSING-BLOCK\n") == std::string::npos,
            "a stub must not print the bare missing-block label");
    }

    // heal: the real block write promotes the asset and the file re-seals
    states[gi] = vfs_pending;
    TEST_CHECK(vfs_state_save(fs_path_join(vfs_get_params().state_dir, "state.bin"), states));
    TEST_CHECK(vfs_materialize(&stats));
    result = vfs_verify_result();
    TEST_CHECK(vfs_verify(&result) >= 0);
    pack = test_find_file(result, prod.pack_name.c_str());
    TEST_CHECK_MSG(pack != nullptr && pack->sealed, "the file must re-seal after the heal");
}

// the journal 'R' op (update resets): replays as a pending reset and a
// later fetch record wins
VFS_TEST(update_journal_reset_op)
{
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "update_journal"));

    vfs_params params;
    params.enabled = true;
    params.offline = true;
    params.install_dir = prod.install;
    params.blobs_dir = prod.blobs;
    params.product_file = fs_path_join(prod.root, "prod.xml");
    vfs_set_params(params);

    VfsPlan plan;
    TEST_CHECK(vfs_plan_ensure(plan));
    const size_t count = vfs_state_asset_count(plan);

    const auto journal_path = fs_path_join(vfs_get_params().state_dir, "journal.log");
    vfs_journal journal;
    TEST_CHECK(vfs_journal_open(journal, journal_path));
    TEST_CHECK(vfs_journal_mark(journal, {'F', 0, 1})); // fetched
    TEST_CHECK(vfs_journal_mark(journal, {'R', 0, 1})); // reset
    TEST_CHECK(vfs_journal_mark(journal, {'F', 0, 1})); // re-fetched: wins
    TEST_CHECK(vfs_journal_mark(journal, {'R', 0, 1})); // reset: wins
    vfs_journal_close(journal);

    std::vector<vfs_journal_record> records;
    TEST_CHECK(vfs_journal_replay(journal_path, records));
    TEST_CHECK_MSG(records.size() == 4, "every record must replay");
    std::vector<uint8_t> states(count, vfs_fetched);
    vfs_state_apply_journal(plan, records, states);
    TEST_CHECK_MSG(states[1] == vfs_pending, "the last reset must win");
}

// --streaming-trap=off: managed files map without protection - reads of
// never-downloaded blocks return the sparse zeros without faulting, the
// ensure path still materializes, and the fault counters stay silent
VFS_TEST(update_trap_off_maps_holes_readable)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "update_trapoff"));

    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));

    vfs_params params = test_fetch_params(prod, srv);
    params.traps = false; // the escape hatch
    vfs_set_params(params);
    TEST_CHECK_MSG(vfs_boot(nullptr), "trap-off boot failed");

    vfs_trap_stats faults_before; // process-global counters: assert the delta
    vfs_trap_stats_get(&faults_before);

    // a pack asset the boot tiers never fetch (T2, blob not in the store)
    uint8_t *base = nullptr;
    size_t size = 0;
    const auto pack_path = fs_path_join(params.install_dir, prod.pack_name);
    TEST_CHECK_MSG(vfs_map_file(pack_path, base, size), "trap-off mapping failed");
    TEST_CHECK_MSG(base != nullptr, "no mapping");

    const auto &asset = vfs_get_params(); // silence unused warnings on gcc
    (void)asset;
    VfsPlan plan;
    TEST_CHECK(vfs_plan_ensure(plan));
    size_t file_index = SIZE_MAX, asset_index = SIZE_MAX;
    for (size_t fi = 0; fi < plan.files.size(); ++fi)
    {
        if (plan.files[fi].name == prod.pack_name)
        {
            file_index = fi;
            asset_index = 0; // part0 (zlib, ml=12): a T2 hole here
            break;
        }
    }
    TEST_CHECK_MSG(file_index != SIZE_MAX, "pack not planned");
    const auto &part = plan.files[file_index].assets[asset_index];
    TEST_CHECK(part.file_offset > 0);

    vfs_trap_stats trap_stats;

    // the hole reads as zeros - no fault, no crash
    TEST_CHECK_MSG(base[part.file_offset] == 0, "an undownloaded block must read as zero");
    vfs_trap_stats_get(&trap_stats);
    TEST_CHECK_MSG(
        trap_stats.faults == faults_before.faults,
        "trap-off must not fault");
    TEST_CHECK_MSG(
        trap_stats.flips == faults_before.flips,
        "trap-off must not flip pages");

    // the ensure path still resolves the range (fetch + materialize + flip)
    TEST_CHECK_MSG(
        vfs_ensure_range(base, size_t(part.file_offset), 64),
        "ensure_range must resolve with traps off");
    TEST_CHECK_MSG(
        vfs_store_pack_asset_present(base, uint64_t(size), asset_index, part),
        "the ensured block must be present after the heal");
    vfs_trap_stats_get(&trap_stats);
    TEST_CHECK_MSG(
        trap_stats.faults == faults_before.faults,
        "trap-off must never count faults");

    vfs_unmap_file(base, size);
    vfs_shutdown();
    test_server_stop(srv);
}

// the sparse probe (deviation from the planned-offset layout): on a
// sparse-capable filesystem (every
// CI filesystem) the probe reports support; the non-sparse warning itself is
// exercised by the manual xuoi --full transcript (no fat volume in CI)
VFS_TEST(update_sparse_probe_reports_support)
{
#if !defined(XUO_WINDOWS)
    TEST_CHECK(vfs_store_fs_supports_sparse(test_temp_root()));
#else
    (void)vfs_store_fs_supports_sparse(test_temp_root()); // exercises the probe
#endif
}

// the deployment step: a recorded CSV converts into the generated
// vfs_manifest_embedded.cpp (via the vuoi binary), and the table it declares
// drives the T1 tier through the embedded-table seam
VFS_TEST(update_embed_record_converts_csv)
{
    test_vfs_shutdown guard;
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "update_embed"));

    test_server srv;
    TEST_CHECK(test_server_start(srv, prod.root, false));
    TEST_CHECK(test_product_patch_repos(
        prod, test_server_url(srv, "/blobs/"), test_server_url(srv, "/blobs/")));

    // a real recorded run: game-demand materializations produce csv rows
    // (boot fetches are cache-warming and stay out of the recording)
    fetch_log_mute mute;
    vfs_params params = test_fetch_params(prod, srv);
    const auto csv = fs_path_join(prod.root, "record.csv");
    params.record_path = csv;
    vfs_set_params(params);
    TEST_CHECK(vfs_boot(nullptr));
    vfs_boot_wait();
    VfsPlan recorded_plan;
    TEST_CHECK_MSG(vfs_plan_ensure(recorded_plan), "plan ensure failed");
    vfs_store_params store_params;
    store_params.blobs_dir = params.blobs_dir;
    store_params.install_dir = params.install_dir;
    std::vector<uint8_t> states;
    size_t pack_index = SIZE_MAX, loose_index = SIZE_MAX;

    for (size_t i = 0; i < recorded_plan.files.size(); ++i)
    {
        if (recorded_plan.files[i].name == prod.pack_name)
            pack_index = i;
        if (recorded_plan.files[i].name == "tiledata.mul")
            loose_index = i;
    }
    TEST_CHECK_MSG(pack_index != SIZE_MAX && loose_index != SIZE_MAX, "plan files missing");
    // demand-resolving an uncached asset needs its blob in the store first
    // (the trap does this transparently; the test fetches explicitly)
    const auto &pack_asset = recorded_plan.files[pack_index].assets[0];
    TEST_CHECK_MSG(
        vfs_fetch_blob_to_store(
            pack_asset.blob.c_str(),
            uint64_t(pack_asset.ct == 0 ? pack_asset.ul : pack_asset.cl),
            false,
            30000),
        "pack blob fetch failed");
    const bool pack_ok = vfs_store_rematerialize_asset(
        recorded_plan, store_params, states, nullptr, uint32_t(pack_index), 0);
    const bool loose_ok = vfs_store_rematerialize_asset(
        recorded_plan, store_params, states, nullptr, uint32_t(loose_index), 0);
    TEST_CHECK_MSG(pack_ok, "pack demand resolution failed");
    TEST_CHECK_MSG(loose_ok, "loose demand resolution failed");
    vfs_shutdown();

    // append duplicates + malformed rows: the converter must skip them
    FILE *fp = fopen(fs_path_ascii(csv), "a");
    TEST_CHECK_MSG(fp != nullptr, "cannot append to the recording");
    fprintf(fp, "%016llx,%s,1,2,0,default\n", // duplicate of the pack part
            (unsigned long long)prod.parts[0].uop_hash(), prod.pack_name.c_str());
    fprintf(fp, "nothex,somepack,1,2,0,default\n"); // malformed hash: skipped
    fclose(fp);

    // convert with the vuoi binary
    std::string out;
    const char *argv[] = { VFS_VUOI_BINARY, "--embed-record", fs_path_ascii(csv), nullptr };
    TEST_CHECK_MSG(test_run_capture(argv, out), "vuoi --embed-record failed to run");
    TEST_CHECK_MSG(
        out.find("SPDX-License-Identifier: AGPL-3.0-or-later") != std::string::npos,
        "the generated file must carry the SPDX header");
    TEST_CHECK_MSG(
        out.find("#include \"vfs_manifest.h\"") != std::string::npos,
        "the generated file must include vfs_manifest.h");
    TEST_CHECK_MSG(
        out.find("vfs_manifest_embedded()") != std::string::npos,
        "the generated file must define vfs_manifest_embedded()");
    // one entry line per distinct recorded asset (pack + loose), in order
    TEST_CHECK_MSG(
        out.find("{ 0, \"\", \"tiledata.mul\" }") != std::string::npos,
        "the loose record must map to the { 0, \"\", name } form");
    char part_line[128];
    snprintf(
        part_line,
        sizeof(part_line),
        "{ 0x%016llx, \"%s\", \"\" }",
        (unsigned long long)prod.parts[0].uop_hash(),
        prod.pack_name.c_str());
    TEST_CHECK_MSG(
        out.find(part_line) != std::string::npos, "the pack record must map to its hash entry");
    TEST_CHECK_MSG(
        out.find("nothex") == std::string::npos, "a malformed hash must be skipped");
    TEST_CHECK_MSG(
        out.find("// from a recorded run; 2 entries") != std::string::npos,
        "the header comment must count exactly the emitted entries");

    // the emitted table drives the tiers: inject it through the seam
    std::vector<vfs_embedded_entry> table;
    size_t pos = 0;
    while ((pos = out.find("{ ", pos)) != std::string::npos)
    {
        const size_t end = out.find("}", pos);
        const std::string line = out.substr(pos, end - pos);
        vfs_embedded_entry entry;
        if (sscanf(line.c_str(), "{ 0x%llx", (unsigned long long *)&entry.uop_hash) == 1)
        {
            const size_t q1 = line.find('"');
            const size_t q2 = line.find('"', q1 + 1);
            entry.uop_file = line.substr(q1 + 1, q2 - q1 - 1);
        }
        else
        {
            entry.uop_hash = 0;
            const size_t q2 = line.rfind('"');
            const size_t q1 = line.rfind('"', q2 - 1);
            entry.name = line.substr(q1 + 1, q2 - q1 - 1);
        }
        table.push_back(entry);
        pos = end;
    }
    TEST_CHECK_MSG(table.size() == 2, "two entries expected, got %zu", table.size());
    vfs_manifest_embedded_set(table);

    // a freshly built plan promotes exactly the recorded pack asset to T1
    vfs_params plan_params;
    plan_params.enabled = true;
    plan_params.offline = true;
    plan_params.install_dir = prod.install;
    plan_params.blobs_dir = prod.blobs;
    plan_params.product_file = fs_path_join(prod.root, "prod.xml");
    vfs_set_params(plan_params);
    VfsPlan plan;
    TEST_CHECK(vfs_plan_ensure(plan));
    vfs_manifest_assign_tiers(plan);
    bool promoted = false;
    for (const auto &file : plan.files)
    {
        for (const auto &asset : file.assets)
        {
            if (asset.uop_hash == prod.parts[0].uop_hash())
            {
                promoted = asset.tier == vfs_tier_t1;
            }
        }
    }
    TEST_CHECK_MSG(promoted, "the recorded pack entry must land in T1");
    vfs_manifest_embedded_set({}); // restore: no override
    test_server_stop(srv);
}

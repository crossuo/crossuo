// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// M1 plan tests: manifest consumption through the mft fetch seam into the
// persisted AssetPlan. Determinism (same inputs -> identical plan bytes),
// planned-offset math (VUOI plan §6.2, §6.3), hole bookkeeping and the real
// M0 fixtures as a product slice.

#define LOGGER_MODULE TestVfs

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include "test_main.h"
#include "test_product.h"

#include <common/logging/logging.h>
#include <xuocore/uop.h>
#include <xuocore/vfs.h>

// mutes the production logging of expected negative-path warnings so the
// suite output stays deterministic
struct plan_log_mute
{
    eLogSystem saved;
    plan_log_mute()
        : saved(g_LogEnabled)
    {
        g_LogEnabled = LogSystemNone;
    }
    ~plan_log_mute() { g_LogEnabled = saved; }
};

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

static const VfsPlan::VfsFile *find_file(const VfsPlan &plan, const char *name)
{
    for (const auto &file : plan.files)
    {
        if (file.name == name)
            return &file;
    }
    return nullptr;
}

// expected pack0.uop layout: header + one section + 4-entry table, then the
// blocks in manifest order; the hole reserves a slot but no bytes
static void check_pack_layout(const VfsPlan &plan, const test_product_data &prod)
{
    const auto *file = find_file(plan, prod.pack_name.c_str());
    TEST_CHECK_MSG(file != nullptr, "pack %s missing from plan", prod.pack_name.c_str());
    TEST_CHECK(file->is_uop);
    TEST_CHECK(file->assets.size() == prod.parts.size());

    const uint64_t table_end =
        sizeof(UopHeader) + sizeof(UopSection) + sizeof(UopFileEntry) * prod.parts.size();
    uint64_t offset = table_end;
    for (size_t i = 0; i < prod.parts.size(); ++i)
    {
        const auto &part = prod.parts[i];
        const auto &asset = file->assets[i];
        TEST_CHECK(asset.uop_hash == part.uop_hash());
        TEST_CHECK(asset.ul == part.payload.size());
        TEST_CHECK(asset.cl == part.blob.size());
        TEST_CHECK(asset.ct == part.ct);
        TEST_CHECK(asset.ml == part.ml);
        TEST_CHECK(asset.mc == part.mc);
        TEST_CHECK(asset.blob == part.blob_rel);
        TEST_CHECK(asset.tier == vfs_tier_t2); // defaulted in M1
        if (part.payload.empty())
        {
            TEST_CHECK_MSG(asset.file_offset == 0, "hole must have no planned offset");
        }
        else
        {
            TEST_CHECK_MSG(
                asset.file_offset == offset,
                "planned offset drift at asset %zu (%" PRIu64 " != %" PRIu64 ")",
                i,
                asset.file_offset,
                offset);
            offset += part.ml + (part.ct == 0 ? part.payload.size() : part.blob.size());
        }
    }
    TEST_CHECK(file->size == offset);
}

// ---------------------------------------------------------------------------
// plan build
// ---------------------------------------------------------------------------

VFS_TEST(plan_build_mini_product)
{
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "plan_build"));

    vfs_manifest_params params;
    params.product_file = fs_path_join(prod.root, "prod.xml");
    params.blobs_dir = prod.blobs;

    VfsPlan plan;
    TEST_CHECK_MSG(vfs_manifest_build(params, plan), "manifest build failed");
    TEST_CHECK(plan.product_serial == prod.serial);

    // the empty pack produces no file; the two loose files follow the pack
    TEST_CHECK(plan.files.size() == 3);
    check_pack_layout(plan, prod);

    const auto *loose = find_file(plan, "tiledata.mul");
    TEST_CHECK(loose != nullptr && !loose->is_uop);
    TEST_CHECK(loose->size == 2048);
    TEST_CHECK(loose->assets.size() == 1);
    TEST_CHECK(loose->assets[0].name == "tiledata.mul");
    TEST_CHECK(loose->assets[0].uop_hash == 0); // loose files carry no uop hash
    TEST_CHECK(loose->assets[0].ct == 0);

    const auto *nested = find_file(plan, "Music/Digital/Britainpos.mp3");
    TEST_CHECK(nested != nullptr && !nested->is_uop);
    TEST_CHECK(nested->assets[0].ct == 1);
    TEST_CHECK(nested->assets[0].ul == 3333);
}

VFS_TEST(plan_missing_manifest_fails)
{
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "plan_missing"));

    // the base package manifest blob is gone: consumption must fail loudly
    fs_del(fs_path_join(prod.blobs, "base", "pkg.mft"));
    TEST_CHECK(!fs_path_exists(fs_path_join(prod.blobs, "base", "pkg.mft")));

    vfs_manifest_params params;
    params.product_file = fs_path_join(prod.root, "prod.xml");
    params.blobs_dir = prod.blobs;
    VfsPlan plan;
    TEST_CHECK(!vfs_manifest_build(params, plan));
}

// ---------------------------------------------------------------------------
// manifest-controlled paths must never escape the install dir / blob store
// ---------------------------------------------------------------------------

VFS_TEST(path_component_validation)
{
    TEST_CHECK(vfs_path_component_ok("tiledata.mul"));
    TEST_CHECK(vfs_path_component_ok("pack0.uop"));
    TEST_CHECK(vfs_path_component_ok("Music/Digital/Britainpos.mp3"));
    TEST_CHECK(vfs_path_component_ok("a.b.c"));

    // anything that could climb out of, or start outside, the join root
    TEST_CHECK(!vfs_path_component_ok(nullptr));
    TEST_CHECK(!vfs_path_component_ok(""));
    TEST_CHECK(!vfs_path_component_ok("/etc/passwd"));
    TEST_CHECK(!vfs_path_component_ok(".."));
    TEST_CHECK(!vfs_path_component_ok("../evil.mul"));
    TEST_CHECK(!vfs_path_component_ok("a/../b.mul"));
    TEST_CHECK(!vfs_path_component_ok("a/.."));
    TEST_CHECK(!vfs_path_component_ok("."));
    TEST_CHECK(!vfs_path_component_ok("./tiledata.mul"));
    TEST_CHECK(!vfs_path_component_ok("a//b.mul"));
    TEST_CHECK(!vfs_path_component_ok("tiledata.mul/"));
    // separators that turn into escapes on the other platform
    TEST_CHECK(!vfs_path_component_ok("bad\\name.mul"));
    TEST_CHECK(!vfs_path_component_ok("C:\\evil.mul"));
    TEST_CHECK(!vfs_path_component_ok("X:evil.mul"));
    // control characters
    TEST_CHECK(!vfs_path_component_ok("bad\tname.mul"));
    TEST_CHECK(!vfs_path_component_ok("bad\nname.mul"));
}

VFS_TEST(plan_skips_unsafe_manifest_paths)
{
    test_product_data prod;
    // a loose file escaping the install dir through its name
    prod.extra_loose.push_back(
        "<f n=\"../../evil.mul\" ul=\"400\" ct=\"0\" cl=\"400\" t=\"6a8dd2b1\"/>");
    // a loose file smuggling a Windows separator
    prod.extra_loose.push_back(
        "<f n=\"bad\\name.mul\" ul=\"400\" ct=\"0\" cl=\"400\" t=\"6a8dd2b1\"/>");
    // a pack whose rpath (the pack name) climbs out of the install dir
    prod.extra_packs.push_back(
        "<p name=\"evil.uop\" rpath=\"../evil.uop\"><files>"
        "<f ph=\"a\" sh=\"b\" ul=\"400\" ct=\"0\" cl=\"400\" ml=\"0\" mc=\"0\" t=\"6a8dd2b1\"/>"
        "</files></p>");

    TEST_CHECK(test_build_product(prod, "plan_traversal"));

    vfs_manifest_params params;
    params.product_file = fs_path_join(prod.root, "prod.xml");
    params.blobs_dir = prod.blobs;

    VfsPlan plan;
    {
        plan_log_mute mute; // the skipped entries log warnings by design
        TEST_CHECK_MSG(vfs_manifest_build(params, plan), "manifest build failed");
    }

    // the plan holds only the sane files; every unsafe entry was skipped and
    // every kept name is a safe join
    TEST_CHECK(plan.files.size() == 3);
    TEST_CHECK(find_file(plan, "../../evil.mul") == nullptr);
    TEST_CHECK(find_file(plan, "bad\\name.mul") == nullptr);
    TEST_CHECK(find_file(plan, "evil.uop") == nullptr);
    for (const auto &file : plan.files)
    {
        TEST_CHECK_MSG(
            vfs_path_component_ok(file.name.c_str()), "unsafe plan file: %s", file.name.c_str());
    }
    check_pack_layout(plan, prod);

    // materializing the plan writes nothing outside the install dir (both
    // escape targets would land in prod.root, one level above it)
    vfs_store_params store_params;
    store_params.blobs_dir = prod.blobs;
    store_params.install_dir = prod.install;
    std::vector<uint8_t> states;
    vfs_journal journal;
    TEST_CHECK(vfs_journal_open(journal, fs_path_join(prod.root, "journal.log")));
    vfs_materialize_stats stats;
    TEST_CHECK(vfs_store_materialize(plan, store_params, states, &journal, &stats));
    TEST_CHECK(vfs_journal_close(journal));
    TEST_CHECK(stats.assets_total == 6);
    TEST_CHECK(stats.assets_fetched == 5);
    TEST_CHECK(!fs_path_exists(fs_path_join(prod.root, "evil.mul")));
    TEST_CHECK(!fs_path_exists(fs_path_join(prod.root, "evil.uop")));
    TEST_CHECK(!fs_path_exists(fs_path_join(prod.install, "bad\\name.mul")));
}

VFS_TEST(plan_deterministic_bytes)
{
    // same inputs, two independent roots: identical plan bytes
    test_product_data prod_a, prod_b;
    TEST_CHECK(test_build_product(prod_a, "plan_det_a"));
    TEST_CHECK(test_build_product(prod_b, "plan_det_b"));

    vfs_manifest_params params;
    params.blobs_dir = prod_a.blobs;
    VfsPlan plan_a, plan_b;
    params.product_file = fs_path_join(prod_a.root, "prod.xml");
    TEST_CHECK(vfs_manifest_build(params, plan_a));
    params.product_file = fs_path_join(prod_b.root, "prod.xml");
    params.blobs_dir = prod_b.blobs;
    TEST_CHECK(vfs_manifest_build(params, plan_b));

    std::vector<uint8_t> bytes_a, bytes_b;
    TEST_CHECK(vfs_plan_serialize(plan_a, bytes_a));
    TEST_CHECK(vfs_plan_serialize(plan_b, bytes_b));
    TEST_CHECK(bytes_a == bytes_b);
    TEST_CHECK(!bytes_a.empty());

    // and rebuilding from the same root is stable as well
    params.product_file = fs_path_join(prod_a.root, "prod.xml");
    params.blobs_dir = prod_a.blobs;
    VfsPlan plan_a2;
    TEST_CHECK(vfs_manifest_build(params, plan_a2));
    std::vector<uint8_t> bytes_a2;
    TEST_CHECK(vfs_plan_serialize(plan_a2, bytes_a2));
    TEST_CHECK(bytes_a == bytes_a2);
}

VFS_TEST(plan_state_roundtrip)
{
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "plan_roundtrip"));

    vfs_manifest_params params;
    params.product_file = fs_path_join(prod.root, "prod.xml");
    params.blobs_dir = prod.blobs;
    VfsPlan plan;
    TEST_CHECK(vfs_manifest_build(params, plan));

    const auto path = fs_path_join(prod.root, "plan.bin");
    TEST_CHECK(vfs_plan_save(path, plan));
    VfsPlan loaded;
    TEST_CHECK_MSG(vfs_plan_load(path, loaded), "plan.bin load failed");

    std::vector<uint8_t> bytes_orig, bytes_loaded;
    TEST_CHECK(vfs_plan_serialize(plan, bytes_orig));
    TEST_CHECK(vfs_plan_serialize(loaded, bytes_loaded));
    TEST_CHECK(bytes_orig == bytes_loaded);
}

// ---------------------------------------------------------------------------
// the real M0 fixtures as a product slice
// ---------------------------------------------------------------------------

VFS_TEST(plan_fixture_product)
{
    test_product_data prod;
    TEST_CHECK(test_build_fixture_product(prod, "plan_fixture"));

    vfs_manifest_params params;
    params.product_file = fs_path_join(prod.root, "prod.xml");
    params.blobs_dir = prod.blobs;
    VfsPlan plan;
    TEST_CHECK_MSG(vfs_manifest_build(params, plan), "fixture manifest build failed");
    TEST_CHECK(plan.product_serial == 1787679382);

    // the captured artLegacyMUL.uop entry:
    // <f ph="3cf84" sh="fc599ef4" ul="1dc2" ct="1" cl="1917" ml="88" mc="9fd7f3c6"/>
    const auto *art = find_file(plan, "artLegacyMUL.uop");
    TEST_CHECK_MSG(art != nullptr, "artLegacyMUL.uop missing from plan");
    TEST_CHECK(art->is_uop);
    TEST_CHECK(art->assets.size() == 1);
    const auto &asset = art->assets[0];
    TEST_CHECK(asset.uop_hash == 0xfc599ef40003cf84ull);
    TEST_CHECK(asset.ul == 7618);
    TEST_CHECK(asset.cl == 6423);
    TEST_CHECK(asset.ct == 1);
    TEST_CHECK(asset.ml == 0x88); // manifest attrs are hex: ml="88" -> 136
    TEST_CHECK(asset.mc == 0x9fd7f3c6);
    TEST_CHECK(asset.blob == "base/artLegacyMUL.uop/0003cf84fc599ef4");
    TEST_CHECK(asset.file_offset == sizeof(UopHeader) + sizeof(UopSection) + sizeof(UopFileEntry));
    TEST_CHECK(art->size == asset.file_offset + asset.ml + asset.cl);

    // the synthesized empty manifests contribute no files
    for (const auto &file : plan.files)
    {
        TEST_CHECK_STR(file.name, "artLegacyMUL.uop");
    }
}

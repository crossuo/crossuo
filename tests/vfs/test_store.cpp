// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// store tests: the materializer against the synthetic mini product and
// the captured live fixture part. Every materialized pack must parse through the
// production path (CFileManager::UopLoadFile) and every present asset must
// decode through CUopMappedFile::GetData; holes stay resolvable-free
// (Offset == 0, skipped) while the rest of the pack keeps working.

#define LOGGER_MODULE TestVfs

#include <stdio.h>
#include <string.h>

#include <stddef.h>
#include <vector>

#include "test_main.h"
#include "test_product.h"

#include <xuocore/common.h>
#include <common/logging/logging.h>
#include <xuocore/uodata.h>
#include <xuocore/uop.h>
#include <xuocore/vfs.h>

// mutes the production logging of load/verify paths (expected errors of
// negative tests) so the suite output stays deterministic
struct store_log_mute
{
    eLogSystem saved;
    store_log_mute()
        : saved(g_LogEnabled)
    {
        g_LogEnabled = LogSystemNone;
    }
    ~store_log_mute() { g_LogEnabled = saved; }
};

// loads a materialized pack through the exact production boot path
static bool load_pack(const test_product_data &prod, const char *name, CUopMappedFile &file)
{
    uo_data_init(fs_path_ascii(prod.install), 0, false);
    store_log_mute mute;
    return g_FileManager.UopLoadFile(file, name, false);
}

static const vfs_verify_asset *find_verify_asset(const vfs_verify_result &result, uint64_t uop_hash)
{
    for (const auto &file : result.files)
    {
        for (const auto &asset : file.assets)
        {
            if (asset.uop_hash == uop_hash)
                return &asset;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// full materialization
// ---------------------------------------------------------------------------

VFS_TEST(store_materialize_full_roundtrip)
{
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "store_full"));

    vfs_manifest_params manifest_params;
    manifest_params.product_file = fs_path_join(prod.root, "prod.xml");
    manifest_params.blobs_dir = prod.blobs;
    VfsPlan plan;
    TEST_CHECK(vfs_manifest_build(manifest_params, plan));

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
    TEST_CHECK(stats.assets_fetched == 5); // the hole is not fetched
    TEST_CHECK(stats.assets_hole == 1);
    TEST_CHECK(stats.assets_missing == 0);
    TEST_CHECK(stats.assets_failed == 0);
    for (auto state : states)
    {
        TEST_CHECK(state == vfs_fetched);
    }

    // production load path over the materialized pack
    CUopMappedFile pack;
    TEST_CHECK_MSG(load_pack(prod, "pack0.uop", pack), "UopLoadFile failed");
    TEST_CHECK(pack.Header->Magic == MYP_MAGIC);
    TEST_CHECK(pack.Header->Version == 4);
    TEST_CHECK(pack.Header->FileCapacity == 4);
    TEST_CHECK(pack.Header->FileCount == 4);
    TEST_CHECK(pack.FileCount() == 3); // the hole entry is not mapped

    // present assets decode to their exact payloads
    for (size_t i = 0; i < 3; ++i)
    {
        const auto &part = prod.parts[i];
        const auto *entry = pack.GetAsset(part.uop_hash());
        TEST_CHECK_MSG(entry != nullptr, "asset %zu missing", i);
        TEST_CHECK(entry->DecompressedSize == part.payload.size());
        // stored layout: entries carry the decompressed payload size, Flags 0
        TEST_CHECK(entry->CompressedSize == part.payload.size());
        TEST_CHECK(entry->Flags == 0);
        TEST_CHECK(entry->Checksum == part.mc);
        TEST_CHECK_MSG(pack.GetData(entry) == part.payload, "payload mismatch at asset %zu", i);
    }

    // the hole: reserved in the table (Offset stays 0) and skipped by the
    // production loader, while the rest of the pack keeps working
    const auto &hole = prod.parts[3];
    TEST_CHECK(pack.GetAsset(hole.uop_hash()) == nullptr);
    TEST_CHECK(!pack.HasAsset(hole.uop_hash()));

    // synthesized metadata: type 3 chain header + zeros
    const auto *first = pack.GetAsset(prod.parts[0].uop_hash());
    const auto meta = pack.GetMeta(first);
    TEST_CHECK(meta.size() == 12);
    TEST_CHECK(meta[0] == 0x03 && meta[1] == 0x00); // type = 3
    TEST_CHECK(meta[2] == 0x08 && meta[3] == 0x00); // size = ml - 4
    TEST_CHECK(meta[4] == 0x00 && meta[11] == 0x00);

    // loose files: whole-file materialization, nested directory included
    for (const auto &loose : prod.loose)
    {
        std::vector<uint8_t> bytes;
        TEST_CHECK_MSG(
            fs_file_read(fs_path_ascii(fs_path_join(prod.install, loose.name)), bytes),
            "loose file missing: %s",
            loose.name.c_str());
        TEST_CHECK(bytes == loose.payload);
    }

    // the empty pack produces no file at all
    TEST_CHECK(!fs_path_exists(fs_path_join(prod.install, "empty.uop")));
}

// ---------------------------------------------------------------------------
// partial materialization, resume and idempotency
// ---------------------------------------------------------------------------

VFS_TEST(store_materialize_partial_resume)
{
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "store_partial"));

    vfs_manifest_params manifest_params;
    manifest_params.product_file = fs_path_join(prod.root, "prod.xml");
    manifest_params.blobs_dir = prod.blobs;
    VfsPlan plan;
    TEST_CHECK(vfs_manifest_build(manifest_params, plan));

    vfs_store_params store_params;
    store_params.blobs_dir = prod.blobs;
    store_params.install_dir = prod.install;

    // the third part's blob is missing from the store
    const auto &missing = prod.parts[2];
    fs_del(fs_path_join(prod.blobs, missing.blob_rel));
    TEST_CHECK(!fs_path_exists(fs_path_join(prod.blobs, missing.blob_rel)));

    std::vector<uint8_t> states;
    vfs_journal journal;
    TEST_CHECK(vfs_journal_open(journal, fs_path_join(prod.root, "journal.log")));
    vfs_materialize_stats stats;
    TEST_CHECK(vfs_store_materialize(plan, store_params, states, &journal, &stats));
    TEST_CHECK(stats.assets_fetched == 4); // A, B + both loose files
    TEST_CHECK(stats.assets_missing == 1);
    TEST_CHECK(vfs_journal_close(journal));
    TEST_CHECK(states[2] == vfs_pending); // stays pending: fetch-engine work

    // the partially materialized pack loads and exposes its present assets
    {
        CUopMappedFile pack;
        TEST_CHECK(load_pack(prod, "pack0.uop", pack));
        TEST_CHECK(pack.GetAsset(prod.parts[0].uop_hash()) != nullptr);
        TEST_CHECK(pack.GetAsset(prod.parts[1].uop_hash()) != nullptr);
        TEST_CHECK(pack.GetAsset(missing.uop_hash()) == nullptr); // open hole
        TEST_CHECK(pack.FileCount() == 2);
        TEST_CHECK(pack.GetData(pack.GetAsset(prod.parts[0].uop_hash())) == prod.parts[0].payload);
    }

    // verify flags exactly the missing asset (and only it)
    vfs_verify_result result;
    TEST_CHECK(vfs_store_verify(plan, store_params, &result) == 0); // missing is not corrupt
    const auto *va = find_verify_asset(result, missing.uop_hash());
    TEST_CHECK(va != nullptr && va->status == vfs_verify_missing_blob);
    TEST_CHECK(result.files[0].counts[vfs_verify_ok] == 3); // A, B + the hole
    TEST_CHECK(result.files[0].counts[vfs_verify_missing_blob] == 1);

    // the blob arrives (the fetch engine would fetch it): re-run completes the pack
    TEST_CHECK(test_write_file(fs_path_join(prod.blobs, missing.blob_rel), missing.blob));
    TEST_CHECK(vfs_journal_open(journal, fs_path_join(prod.root, "journal.log")));
    TEST_CHECK(vfs_store_materialize(plan, store_params, states, &journal, &stats));
    TEST_CHECK(vfs_journal_close(journal));
    TEST_CHECK(stats.assets_fetched == 1);
    TEST_CHECK(states[2] == vfs_fetched);

    TEST_CHECK(vfs_store_verify(plan, store_params, &result) == 0);
    TEST_CHECK(result.files[0].sealed);
    TEST_CHECK(result.files[0].counts[vfs_verify_ok] == 4); // hole counts as ok

    // a third run is a no-op: everything is present, nothing rewritten
    TEST_CHECK(vfs_journal_open(journal, fs_path_join(prod.root, "journal.log")));
    vfs_materialize_stats noop;
    TEST_CHECK(vfs_store_materialize(plan, store_params, states, &journal, &noop));
    TEST_CHECK(vfs_journal_close(journal));
    TEST_CHECK(noop.bytes_written == 0);
    TEST_CHECK(noop.assets_present == 5);
    TEST_CHECK(noop.assets_fetched == 0);
}

VFS_TEST(store_verify_detects_corruption)
{
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "store_corrupt"));

    vfs_manifest_params manifest_params;
    manifest_params.product_file = fs_path_join(prod.root, "prod.xml");
    manifest_params.blobs_dir = prod.blobs;
    VfsPlan plan;
    TEST_CHECK(vfs_manifest_build(manifest_params, plan));

    vfs_store_params store_params;
    store_params.blobs_dir = prod.blobs;
    store_params.install_dir = prod.install;

    std::vector<uint8_t> states;
    vfs_journal journal;
    TEST_CHECK(vfs_journal_open(journal, fs_path_join(prod.root, "journal.log")));
    TEST_CHECK(vfs_store_materialize(plan, store_params, states, &journal, nullptr));
    TEST_CHECK(vfs_journal_close(journal));

    // flip a payload byte inside the zlib-compressed third block: the bytes
    // are there, so the zlib stream mismatch must be flagged CORRUPT (stored
    // blocks carry no payload checksum, their check is size-based)
    const auto &victim = prod.parts[2];
    const auto victim_offset =
        plan.files[0].assets[2].file_offset + victim.ml; // payload, not metadata
    const auto pack_path = fs_path_join(prod.install, "pack0.uop");
    {
        FILE *fp = fopen(fs_path_ascii(pack_path), "r+b");
        TEST_CHECK_MSG(fp != nullptr, "could not open pack for corruption");
        TEST_CHECK(fseek(fp, long(victim_offset), SEEK_SET) == 0);
        const uint8_t flipped = uint8_t(victim.payload[0] ^ 0xff);
        TEST_CHECK(fwrite(&flipped, 1, 1, fp) == 1);
        fclose(fp);
    }

    vfs_verify_result result;
    {
        store_log_mute mute;
        TEST_CHECK(vfs_store_verify(plan, store_params, &result) == 1); // the flipped byte
    }
    TEST_CHECK_MSG(result.corrupted, "flipped byte must be flagged CORRUPT");
    const auto *va = find_verify_asset(result, victim.uop_hash());
    TEST_CHECK(va != nullptr && va->status == vfs_verify_corrupt);

    // a truncated loose file is corrupt as well
    const auto loose_path = fs_path_join(prod.install, "tiledata.mul");
    fs_del(loose_path);
    TEST_CHECK(!fs_path_exists(loose_path));
    {
        std::vector<uint8_t> half(prod.loose[0].payload.size() / 2, 0xab);
        TEST_CHECK(fs_file_write(fs_path_ascii(loose_path), half));
    }
    {
        store_log_mute mute;
        // the pack block and the truncated loose file are both corrupt
        TEST_CHECK(vfs_store_verify(plan, store_params, &result) == 2);
    }
    TEST_CHECK(result.corrupted);
    const auto *vl = find_verify_asset(result, 0); // loose assets have hash 0
    TEST_CHECK(vl != nullptr && vl->status == vfs_verify_corrupt);
}

// ---------------------------------------------------------------------------
// the captured live fixture part through the whole pipeline
// ---------------------------------------------------------------------------

VFS_TEST(store_fixture_art_legacy)
{
    test_product_data prod;
    TEST_CHECK(test_build_fixture_product(prod, "store_fixture"));

    vfs_manifest_params manifest_params;
    manifest_params.product_file = fs_path_join(prod.root, "prod.xml");
    manifest_params.blobs_dir = prod.blobs;
    VfsPlan plan;
    TEST_CHECK(vfs_manifest_build(manifest_params, plan));

    vfs_store_params store_params;
    store_params.blobs_dir = prod.blobs;
    store_params.install_dir = prod.install;

    std::vector<uint8_t> states;
    vfs_journal journal;
    TEST_CHECK(vfs_journal_open(journal, fs_path_join(prod.root, "journal.log")));
    vfs_materialize_stats stats;
    TEST_CHECK(vfs_store_materialize(plan, store_params, states, &journal, &stats));
    TEST_CHECK(vfs_journal_close(journal));
    TEST_CHECK(stats.assets_fetched == 1);

    // production load path: exactly one asset, decoding to the 7618 payload
    CUopMappedFile pack;
    TEST_CHECK_MSG(load_pack(prod, "artLegacyMUL.uop", pack), "UopLoadFile failed");
    TEST_CHECK(pack.FileCount() == 1);
    const auto *entry = pack.GetAsset(0xfc599ef40003cf84ull);
    TEST_CHECK_MSG(entry != nullptr, "fixture asset hash not resolvable");
    const auto data = pack.GetData(entry);
    TEST_CHECK(data.size() == FIXTURE_ART_PAYLOAD);
    TEST_CHECK(entry->Flags == 0);
    TEST_CHECK(entry->Checksum == 0x9fd7f3c6);

    // the pack loads with a hole for everything not yet materialized
    vfs_verify_result result;
    TEST_CHECK(vfs_store_verify(plan, store_params, &result) == 0);
    TEST_CHECK(!result.corrupted);
    TEST_CHECK(result.files[0].sealed);
}

// ---------------------------------------------------------------------------
// the plan fingerprint in UopHeader::Signature
// ---------------------------------------------------------------------------

VFS_TEST(store_pack_signature_fingerprint)
{
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "store_signature"));

    vfs_manifest_params manifest_params;
    manifest_params.product_file = fs_path_join(prod.root, "prod.xml");
    manifest_params.blobs_dir = prod.blobs;
    VfsPlan plan;
    TEST_CHECK(vfs_manifest_build(manifest_params, plan));

    vfs_store_params store_params;
    store_params.blobs_dir = prod.blobs;
    store_params.install_dir = prod.install;

    std::vector<uint8_t> states;
    vfs_journal journal;
    TEST_CHECK(vfs_journal_open(journal, fs_path_join(prod.root, "journal.log")));
    TEST_CHECK(vfs_store_materialize(plan, store_params, states, &journal, nullptr));
    TEST_CHECK(vfs_journal_close(journal));

    const auto pack_path = fs_path_join(prod.install, "pack0.uop");
    const auto read_signature = [&pack_path]() -> uint32_t
    {
        FILE *fp = fopen(fs_path_ascii(pack_path), "rb");
        if (!fp)
        {
            vfs_test_fail(__FILE__, __LINE__, "pack missing");
            return 0;
        }
        UopHeader header;
        memset(&header, 0, sizeof(header));
        const bool ok = fread(&header, sizeof(header), 1, fp) == 1;
        fclose(fp);
        if (!ok)
        {
            vfs_test_fail(__FILE__, __LINE__, "short header read");
            return 0;
        }
        return header.Signature;
    };

    // the fresh pack is stamped with its plan fingerprint
    const uint32_t signature = read_signature();
    TEST_CHECK(signature != 0);

    // a stale signature (an older or foreign table) forces a recreate on
    // open: the table is rebuilt and every block is written again, even
    // though the total file size matches the plan
    {
        FILE *fp = fopen(fs_path_ascii(pack_path), "r+b");
        TEST_CHECK_MSG(fp != nullptr, "could not open pack for stamping");
        const uint32_t zero = 0;
        TEST_CHECK(fseek(fp, long(offsetof(UopHeader, Signature)), SEEK_SET) == 0);
        TEST_CHECK(fwrite(&zero, 1, sizeof(zero), fp) == sizeof(zero));
        fclose(fp);
    }
    vfs_materialize_stats stats;
    TEST_CHECK(vfs_journal_open(journal, fs_path_join(prod.root, "journal.log")));
    TEST_CHECK(vfs_store_materialize(plan, store_params, states, &journal, &stats));
    TEST_CHECK(vfs_journal_close(journal));
    // the recreated pack's blocks were rewritten even though the DB said
    // fetched; the loose files were untouched and stay "present"
    TEST_CHECK(stats.assets_fetched == 3);
    TEST_CHECK(stats.assets_present == 2);
    TEST_CHECK(read_signature() == signature);

    // a different plan with the same file size must be told apart too:
    // re-point one entry's hash (sizes and offsets are unchanged)
    VfsPlan drifted = plan;
    const uint64_t old_hash = drifted.files[0].assets[0].uop_hash;
    const uint64_t new_hash = 0xdeadbeefcafe0001ull;
    drifted.files[0].assets[0].uop_hash = new_hash;
    states.clear(); // fresh DB: every asset goes through the write path
    TEST_CHECK(vfs_journal_open(journal, fs_path_join(prod.root, "journal.log")));
    TEST_CHECK(vfs_store_materialize(drifted, store_params, states, &journal, &stats));
    TEST_CHECK(vfs_journal_close(journal));
    TEST_CHECK(stats.assets_fetched == 5); // the stale table was discarded
    TEST_CHECK(read_signature() != signature);

    // the on-disk table follows the new plan: no stale entry survives
    {
        CUopMappedFile pack;
        TEST_CHECK_MSG(load_pack(prod, "pack0.uop", pack), "UopLoadFile failed");
        TEST_CHECK(pack.GetAsset(old_hash) == nullptr);
        const auto *entry = pack.GetAsset(new_hash);
        TEST_CHECK_MSG(entry != nullptr, "drifted asset hash missing");
        TEST_CHECK(pack.GetData(entry) == prod.parts[0].payload);
    }

    // and an unchanged re-run now reuses the compatible table
    TEST_CHECK(vfs_journal_open(journal, fs_path_join(prod.root, "journal.log")));
    vfs_materialize_stats noop;
    TEST_CHECK(vfs_store_materialize(drifted, store_params, states, &journal, &noop));
    TEST_CHECK(vfs_journal_close(journal));
    TEST_CHECK(noop.bytes_written == 0);
    TEST_CHECK(noop.assets_present == 5);
}

// ---------------------------------------------------------------------------
// backend interface smoke test (store_sparse)
// ---------------------------------------------------------------------------

VFS_TEST(store_backend_sparse)
{
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "store_backend"));

    vfs_manifest_params manifest_params;
    manifest_params.product_file = fs_path_join(prod.root, "prod.xml");
    manifest_params.blobs_dir = prod.blobs;
    VfsPlan plan;
    TEST_CHECK(vfs_manifest_build(manifest_params, plan));

    vfs_store_params store_params;
    store_params.blobs_dir = prod.blobs;
    store_params.install_dir = prod.install;

    vfs_store_backend *backend = vfs_store_backend_sparse_create(store_params);
    TEST_CHECK(backend != nullptr);
    TEST_CHECK_MSG(backend->open(plan), "backend open failed");

    const auto &part = plan.files[0].assets[0];
    const auto &hole = plan.files[0].assets[3];
    const auto &loose = plan.files[1].assets[0];

    TEST_CHECK(!backend->is_ready(part));
    backend->prefetch(part, 0); // no-op (store-only materialization)
    backend->evict(0);          // no-op (disk files are never evicted)
    TEST_CHECK_MSG(backend->ensure(part, true), "ensure failed for blob-backed asset");
    TEST_CHECK(backend->is_ready(part));
    TEST_CHECK(backend->ensure(hole, true)); // holes are trivially ready
    TEST_CHECK(backend->is_ready(hole));
    TEST_CHECK_MSG(backend->ensure(loose, true), "ensure failed for loose asset");
    TEST_CHECK(backend->is_ready(loose));
    backend->seal_check();

    // the materialized pack parses with the production loader
    CUopMappedFile pack;
    TEST_CHECK(load_pack(prod, "pack0.uop", pack));
    TEST_CHECK(pack.GetData(pack.GetAsset(prod.parts[0].uop_hash())) == prod.parts[0].payload);

    delete backend;
}

// ---------------------------------------------------------------------------
// the reconcile-grade loose content check
// ---------------------------------------------------------------------------

static const VfsAsset *find_loose_asset(const VfsPlan &plan, const char *name)
{
    for (const auto &file : plan.files)
    {
        if (!file.is_uop && file.name == name && !file.assets.empty())
            return &file.assets[0];
    }
    return nullptr;
}

// direct test of the cheap reconcile-grade presence check for loose files:
// head compare (ct=0) or bounded inflate (ct=1) against the store blob;
// a missing blob or a short file never proves presence
VFS_TEST(store_loose_content_check)
{
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "store_loose_ok")); // ct=0 + ct=1 loose files

    vfs_manifest_params manifest_params;
    manifest_params.product_file = fs_path_join(prod.root, "prod.xml");
    manifest_params.blobs_dir = prod.blobs;
    VfsPlan plan;
    TEST_CHECK(vfs_manifest_build(manifest_params, plan));

    const auto *raw = find_loose_asset(plan, "tiledata.mul");
    const auto *packed = find_loose_asset(plan, "Music/Digital/Britainpos.mp3");
    TEST_CHECK_MSG(raw != nullptr && packed != nullptr, "both loose assets must be planned");
    TEST_CHECK(raw->ct == 0 && packed->ct == 1);

    const auto raw_path = fs_path_join(prod.install, "tiledata.mul");
    const auto packed_path = fs_path_join(prod.install, "Music/Digital/Britainpos.mp3");

    // ct=0: a matching head passes, one flipped head byte fails
    TEST_CHECK(test_write_file(raw_path, prod.loose[0].payload));
    TEST_CHECK(vfs_store_loose_asset_content_ok(raw_path, *raw, prod.blobs));
    std::vector<uint8_t> flipped = prod.loose[0].payload;
    flipped[0] ^= 0xff;
    TEST_CHECK(test_write_file(raw_path, flipped));
    TEST_CHECK(!vfs_store_loose_asset_content_ok(raw_path, *raw, prod.blobs));

    // ct=1: the blob inflates (just the head bytes) to the file's head
    TEST_CHECK(test_write_file(packed_path, prod.loose[1].payload));
    TEST_CHECK(vfs_store_loose_asset_content_ok(packed_path, *packed, prod.blobs));
    flipped = prod.loose[1].payload;
    flipped[7] ^= 0xff; // inside the compared head window
    TEST_CHECK(test_write_file(packed_path, flipped));
    TEST_CHECK(!vfs_store_loose_asset_content_ok(packed_path, *packed, prod.blobs));

    // a missing blob never proves presence, even over a correct file
    TEST_CHECK(test_write_file(raw_path, prod.loose[0].payload));
    test_product_remove_blob(prod, prod.loose[0].blob_rel);
    TEST_CHECK(!vfs_store_loose_asset_content_ok(raw_path, *raw, prod.blobs));

    // a short file cannot supply the compared head bytes
    const std::vector<uint8_t> short_file(
        prod.loose[1].payload.begin(), prod.loose[1].payload.begin() + 8);
    TEST_CHECK(test_write_file(packed_path, short_file));
    TEST_CHECK(!vfs_store_loose_asset_content_ok(packed_path, *packed, prod.blobs));
}

// the ambiguous zero head: a real payload may legitimately start with a long
// zero run (unifont*.mul), which is byte-identical to the sparse skeleton at
// the head window. The check must keep sampling the deeper windows in that
// case - and only in that case (the head decides for every nonzero payload)
VFS_TEST(store_loose_content_zero_head_ambiguity)
{
    test_product_data prod;
    test_add_part(prod, 0x00000001, 0x11111111, 1, 12, 4096); // the pack needs a part
    // 64KB payload: first 16KB zero, the rest the usual pattern - the head and
    // quarter windows read as zeros, the middle/tail windows do not
    const size_t size = 0x10000;
    std::vector<uint8_t> zero_head(size, 0);
    const auto tail_pattern = test_pattern(size);
    for (size_t i = 0x4000; i < size; ++i)
    {
        zero_head[i] = tail_pattern[i];
    }
    for (uint32_t ct = 0; ct <= 1; ++ct)
    {
        char name[64];
        snprintf(name, sizeof(name), "zerohop%d.mul", ct);
        test_add_loose(prod, name, ct, 0);
        auto &loose = prod.loose.back();
        loose.payload = zero_head;
        loose.blob = ct == 1 ? test_zlib(zero_head) : zero_head;
    }
    TEST_CHECK(test_build_product(prod, "store_loose_zero_head"));

    vfs_manifest_params manifest_params;
    manifest_params.product_file = fs_path_join(prod.root, "prod.xml");
    manifest_params.blobs_dir = prod.blobs;
    VfsPlan plan;
    TEST_CHECK(vfs_manifest_build(manifest_params, plan));

    for (uint32_t ct = 0; ct <= 1; ++ct)
    {
        char name[64];
        snprintf(name, sizeof(name), "zerohop%d.mul", ct);
        const auto *asset = find_loose_asset(plan, name);
        TEST_CHECK_MSG(asset != nullptr, "%s must be planned", name);
        const auto path = fs_path_join(prod.install, name);

        // the real payload: ambiguous at the head, proven by the deep windows
        TEST_CHECK(test_write_file(path, zero_head));
        TEST_CHECK(vfs_store_loose_asset_content_ok(path, *asset, prod.blobs));

        // the sparse skeleton (all zeros, right size): identical at the head,
        // rejected by the deep windows - this is the case the multi-window
        // compare exists for, and it must NOT early-exit on the head
        std::vector<uint8_t> skeleton(size, 0);
        TEST_CHECK(test_write_file(path, skeleton));
        TEST_CHECK(!vfs_store_loose_asset_content_ok(path, *asset, prod.blobs));

        // a real payload with one flipped deep byte is caught too (the deep
        // windows genuinely ran): flip inside the middle window
        std::vector<uint8_t> corrupted = zero_head;
        corrupted[0x8000] ^= 0xff;
        TEST_CHECK(test_write_file(path, corrupted));
        TEST_CHECK(!vfs_store_loose_asset_content_ok(path, *asset, prod.blobs));
    }
}

// ---------------------------------------------------------------------------
// range assets: block windows ([ml metadata][payload]) of a larger source
// blob - what plan refinement produces for whole-file UOP parts
// ---------------------------------------------------------------------------

VFS_TEST(store_range_asset_materialize)
{
    test_product_data prod;
    TEST_CHECK(test_build_product(prod, "store_range")); // roots + store dirs

    // a source blob holding two blocks at nonzero offsets, junk-separated:
    // [16 junk][12 meta][payload1][5 pad][12 meta][payload2]
    const std::vector<uint8_t> meta1(12, 0xAA), meta2(12, 0xBB);
    const std::vector<uint8_t> payload1 = { 1, 2, 3, 4, 5, 6, 7, 8 };
    const std::vector<uint8_t> payload2 = { 9, 10, 11, 12 };
    const uint64_t off1 = 16;
    const uint64_t off2 = off1 + meta1.size() + payload1.size() + 5;
    std::vector<uint8_t> source(size_t(off2 + meta2.size() + payload2.size()), 0x00);
    memcpy(source.data() + off1, meta1.data(), meta1.size());
    memcpy(source.data() + off1 + meta1.size(), payload1.data(), payload1.size());
    memcpy(source.data() + off2, meta2.data(), meta2.size());
    memcpy(source.data() + off2 + meta2.size(), payload2.data(), payload2.size());
    const auto src_path = fs_path_join(prod.blobs, "base", "unpacked", "rangesrc");
    TEST_CHECK(fs_path_create(fs_directory(src_path)));
    TEST_CHECK(fs_file_write(fs_path_ascii(src_path), source));

    // hand-built plan: one pack, both assets ranged into the source blob
    VfsPlan plan;
    VfsPlan::VfsFile file;
    file.name = "range.uop";
    file.is_uop = true;
    VfsAsset a1;
    a1.uop_hash = 0xAAAA000000000001;
    a1.ul = payload1.size();
    a1.cl = a1.ul;
    a1.ct = 0;
    a1.ml = uint32_t(meta1.size());
    a1.mc = 0x11111111;
    a1.blob = "base/unpacked/rangesrc";
    a1.blob_offset = off1;
    file.assets.push_back(a1);
    VfsAsset a2 = a1;
    a2.uop_hash = 0xAAAA000000000002;
    a2.ul = payload2.size();
    a2.cl = a2.ul;
    a2.mc = 0x22222222;
    a2.blob_offset = off2;
    file.assets.push_back(a2);
    const uint64_t table = sizeof(UopHeader) + sizeof(UopSection) + sizeof(UopFileEntry) * 2;
    file.assets[0].file_offset = table;
    file.assets[1].file_offset = table + file.assets[0].ml + file.assets[0].ul;
    file.size = file.assets[1].file_offset + file.assets[1].ml + file.assets[1].ul;
    plan.files.push_back(file);

    vfs_store_params store_params;
    store_params.blobs_dir = prod.blobs;
    store_params.install_dir = prod.install;
    TEST_CHECK(vfs_store_prepare(plan, store_params));

    // asset 0 materializes from the store (the ranged blob read)
    std::vector<uint8_t> states;
    vfs_journal journal;
    TEST_CHECK(vfs_journal_open(journal, fs_path_join(prod.root, "journal.log")));
    TEST_CHECK_MSG(
        vfs_store_rematerialize_asset(plan, store_params, states, &journal, 0, 0),
        "ranged rematerialize failed");
    TEST_CHECK(states.size() == 2 && states[0] == vfs_fetched && states[1] == vfs_pending);

    // asset 1 materializes from caller-held bytes (the trap's range fetch)
    std::vector<uint8_t> block2(meta2.size() + payload2.size(), 0);
    memcpy(block2.data(), meta2.data(), meta2.size());
    memcpy(block2.data() + meta2.size(), payload2.data(), payload2.size());
    TEST_CHECK_MSG(
        vfs_store_materialize_asset_data(plan, store_params, states, &journal, 0, 1, block2),
        "data materialize failed");
    TEST_CHECK(states[1] == vfs_fetched);
    TEST_CHECK(vfs_journal_close(journal));

    // the materialized pack parses and decodes through the production path
    CUopMappedFile pack;
    TEST_CHECK_MSG(load_pack(prod, "range.uop", pack), "UopLoadFile failed");
    const auto *e1 = pack.GetAsset(a1.uop_hash);
    const auto *e2 = pack.GetAsset(a2.uop_hash);
    TEST_CHECK_MSG(e1 != nullptr && e2 != nullptr, "range blocks missing from the table");
    TEST_CHECK(e1->Offset == file.assets[0].file_offset && e1->Flags == 0);
    TEST_CHECK(pack.GetData(e1) == payload1);
    TEST_CHECK(pack.GetData(e2) == payload2);
    // the metadata transplant: range blocks keep their source metadata bytes
    const auto meta = pack.GetMeta(e1);
    TEST_CHECK(meta.size() == meta1.size() && memcmp(meta.data(), meta1.data(), meta1.size()) == 0);
    const auto meta_b = pack.GetMeta(e2);
    TEST_CHECK(
        meta_b.size() == meta2.size() && memcmp(meta_b.data(), meta2.data(), meta2.size()) == 0);
}

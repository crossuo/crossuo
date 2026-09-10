// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// state DB tests: plan.bin/state.bin serialization, the fsync'd journal
// and crash reconciliation. The materialized files are the
// source of truth: reconcile must downgrade DB state that the on-disk files
// do not confirm, and a later materialize pass must repair it.

#define LOGGER_MODULE TestVfs

#include <stdio.h>
#include <string.h>

#include <vector>

#include "test_main.h"
#include "test_product.h"

#include <xuocore/common.h>
#include <common/logging/logging.h>
#include <xuocore/uodata.h>
#include <xuocore/uop.h>
#include <xuocore/vfs.h>

struct state_log_mute
{
    eLogSystem saved;
    state_log_mute()
        : saved(g_LogEnabled)
    {
        g_LogEnabled = LogSystemNone;
    }
    ~state_log_mute() { g_LogEnabled = saved; }
};

static bool build_plan(test_product_data &prod, const char *name, VfsPlan &plan)
{
    if (!test_build_product(prod, name))
        return false;
    vfs_manifest_params params;
    params.product_file = fs_path_join(prod.root, "prod.xml");
    params.blobs_dir = prod.blobs;
    return vfs_manifest_build(params, plan);
}

static void
materialize(const test_product_data &prod, const VfsPlan &plan, std::vector<uint8_t> &states)
{
    vfs_store_params store_params;
    store_params.blobs_dir = prod.blobs;
    store_params.install_dir = prod.install;
    vfs_journal journal;
    TEST_CHECK(vfs_journal_open(journal, fs_path_join(prod.root, "journal.log")));
    TEST_CHECK(vfs_store_materialize(plan, store_params, states, &journal, nullptr));
    TEST_CHECK(vfs_journal_close(journal));
}

// rewrites the pack file exactly as a fresh (empty) table would look like:
// full planned size, complete table, every Offset reset to 0 - the state a
// crash right after table creation would leave behind
static void reset_pack_table(const test_product_data &prod, const VfsPlan &plan)
{
    const auto *file = &plan.files[0];
    const auto path = fs_path_join(prod.install, file->name);
    FILE *fp = fopen(fs_path_ascii(path), "r+b");
    TEST_CHECK_MSG(fp != nullptr, "could not reopen pack");
    const uint64_t table_start = sizeof(UopHeader) + sizeof(UopSection);
    for (size_t i = 0; i < file->assets.size(); ++i)
    {
        const uint64_t pos = table_start + sizeof(UopFileEntry) * i;
        const uint64_t zero = 0;
        TEST_CHECK(fseek(fp, long(pos), SEEK_SET) == 0);
        TEST_CHECK(fwrite(&zero, 1, sizeof(zero), fp) == sizeof(zero));
    }
    fclose(fp);
}

// ---------------------------------------------------------------------------
// plan.bin / state.bin serialization
// ---------------------------------------------------------------------------

VFS_TEST(state_plan_serialization)
{
    test_product_data prod;
    VfsPlan plan;
    TEST_CHECK(build_plan(prod, "state_plan", plan));

    std::vector<uint8_t> bytes;
    TEST_CHECK(vfs_plan_serialize(plan, bytes));
    VfsPlan decoded;
    TEST_CHECK_MSG(vfs_plan_deserialize(bytes.data(), bytes.size(), decoded), "deserialize failed");
    std::vector<uint8_t> bytes_again;
    TEST_CHECK(vfs_plan_serialize(decoded, bytes_again));
    TEST_CHECK(bytes == bytes_again);

    // truncated or garbage input must fail cleanly, not crash
    TEST_CHECK(!vfs_plan_deserialize(bytes.data(), bytes.size() / 2, decoded));
    const std::vector<uint8_t> garbage(bytes.size(), 0x5a);
    TEST_CHECK(!vfs_plan_deserialize(garbage.data(), garbage.size(), decoded));

    // file round trip
    const auto path = fs_path_join(prod.root, "plan.bin");
    TEST_CHECK(vfs_plan_save(path, plan));
    VfsPlan loaded;
    TEST_CHECK(vfs_plan_load(path, loaded));
    TEST_CHECK(loaded.product_serial == plan.product_serial);
    TEST_CHECK(loaded.files.size() == plan.files.size());
}

VFS_TEST(state_states_serialization)
{
    test_product_data prod;
    VfsPlan plan;
    TEST_CHECK(build_plan(prod, "state_states", plan));

    const size_t count = vfs_state_asset_count(plan);
    TEST_CHECK(count == 6);
    std::vector<uint8_t> states(count, vfs_pending);
    states[1] = vfs_fetched;
    states[5] = vfs_failed;

    const auto path = fs_path_join(prod.root, "state.bin");
    TEST_CHECK(vfs_state_save(path, states));
    std::vector<uint8_t> loaded;
    TEST_CHECK(vfs_state_load(path, count, loaded));
    TEST_CHECK(loaded == states);

    // wrong count (a plan drift): unusable, back to all-pending
    std::vector<uint8_t> fallback;
    TEST_CHECK(vfs_state_load(path, count + 1, fallback));
    TEST_CHECK(fallback.size() == count + 1);
    for (auto state : fallback)
    {
        TEST_CHECK(state == vfs_pending);
    }

    // missing file: fresh install, all pending
    std::vector<uint8_t> fresh;
    TEST_CHECK(vfs_state_load(fs_path_join(prod.root, "not-there.bin"), count, fresh));
    TEST_CHECK(fresh.size() == count);
}

// ---------------------------------------------------------------------------
// journal
// ---------------------------------------------------------------------------

VFS_TEST(state_journal_replay)
{
    test_product_data prod;
    VfsPlan plan;
    TEST_CHECK(build_plan(prod, "state_journal", plan));

    const auto path = fs_path_join(prod.root, "journal.log");
    vfs_journal journal;
    TEST_CHECK(vfs_journal_open(journal, path));
    vfs_journal_record fetched = { 'F', 0, 1 };
    vfs_journal_record sealed = { 'S', 0, 0 };
    TEST_CHECK(vfs_journal_mark(journal, fetched));
    TEST_CHECK(vfs_journal_mark(journal, sealed));
    TEST_CHECK(vfs_journal_close(journal));

    std::vector<vfs_journal_record> records;
    TEST_CHECK(vfs_journal_replay(path, records));
    TEST_CHECK(records.size() == 2);
    TEST_CHECK(records[0].op == 'F');
    TEST_CHECK(records[0].file_index == 0);
    TEST_CHECK(records[0].asset_index == 1);
    TEST_CHECK(records[1].op == 'S');
    TEST_CHECK(records[1].file_index == 0);

    // append keeps history (an existing journal is never truncated)
    TEST_CHECK(vfs_journal_open(journal, path));
    vfs_journal_record second = { 'F', 2, 0 };
    TEST_CHECK(vfs_journal_mark(journal, second));
    TEST_CHECK(vfs_journal_close(journal));
    TEST_CHECK(vfs_journal_replay(path, records));
    TEST_CHECK(records.size() == 3);
    TEST_CHECK(records[2].op == 'F');
    TEST_CHECK(records[2].file_index == 2);

    // a torn trailing line is ignored
    FILE *fp = fopen(fs_path_ascii(path), "a");
    TEST_CHECK(fp != nullptr);
    fprintf(fp, "F 9"); // no newline: a crash mid-write
    fclose(fp);
    TEST_CHECK(vfs_journal_replay(path, records));
    TEST_CHECK(records.size() == 3);
}

VFS_TEST(state_apply_journal)
{
    test_product_data prod;
    VfsPlan plan;
    TEST_CHECK(build_plan(prod, "state_apply", plan));

    std::vector<vfs_journal_record> records = {
        { 'F', 0, 0 }, { 'F', 0, 3 }, { 'F', 1, 0 }, { 'S', 0, 0 }, { 'F', 9, 9 }
    };
    std::vector<uint8_t> states(vfs_state_asset_count(plan), vfs_pending);
    vfs_state_apply_journal(plan, records, states);

    // pack0.uop parts A and the hole are fetched, the loose file is fetched,
    // the out-of-range record is ignored and everything else stays pending
    TEST_CHECK(states[0] == vfs_fetched); // part A
    TEST_CHECK(states[1] == vfs_pending); // part B
    TEST_CHECK(states[2] == vfs_pending); // part C
    TEST_CHECK(states[3] == vfs_fetched); // hole D
    TEST_CHECK(states[4] == vfs_fetched); // tiledata.mul
    TEST_CHECK(states[5] == vfs_pending); // Britainpos.mp3
}

// ---------------------------------------------------------------------------
// crash reconciliation
// ---------------------------------------------------------------------------

VFS_TEST(state_reconcile_confirms_and_downgrades)
{
    test_product_data prod;
    VfsPlan plan;
    TEST_CHECK(build_plan(prod, "state_reconcile", plan));

    std::vector<uint8_t> states;
    materialize(prod, plan, states);
    for (auto state : states)
    {
        TEST_CHECK(state == vfs_fetched);
    }

    // a fully consistent DB reconciles with zero repairs
    {
        state_log_mute mute;
        vfs_reconcile_stats stats;
        TEST_CHECK(vfs_state_reconcile(plan, prod.install, states, &stats));
        TEST_CHECK(stats.assets_checked == 6);
        TEST_CHECK(stats.assets_repaired == 0);
        TEST_CHECK(stats.assets_upgraded == 0);
    }

    // simulate the crash window: the table was (re)written but the entry
    // offsets are not patched yet while the DB still says fetched
    reset_pack_table(prod, plan);
    {
        state_log_mute mute;
        vfs_reconcile_stats stats;
        TEST_CHECK(vfs_state_reconcile(plan, prod.install, states, &stats));
        TEST_CHECK(stats.assets_repaired == 3); // parts A, B, C of the pack
    }
    TEST_CHECK(states[0] == vfs_pending);
    TEST_CHECK(states[1] == vfs_pending);
    TEST_CHECK(states[2] == vfs_pending);
    TEST_CHECK(states[3] == vfs_fetched); // the hole needs no bytes
    TEST_CHECK(states[4] == vfs_fetched); // loose files unaffected
    TEST_CHECK(states[5] == vfs_fetched);

    // the interrupted materialize completes on re-run, byte-exact payloads
    materialize(prod, plan, states);
    for (auto state : states)
    {
        TEST_CHECK(state == vfs_fetched);
    }

    // a lost state file with all bytes on disk: reconcile rebuilds the DB
    // from the files (pending -> fetched confirmations)
    std::vector<uint8_t> lost(states.size(), vfs_pending);
    {
        state_log_mute mute;
        vfs_reconcile_stats stats;
        TEST_CHECK(vfs_state_reconcile(plan, prod.install, lost, &stats));
        TEST_CHECK(stats.assets_upgraded == 6); // 5 written + the hole
    }
    TEST_CHECK(lost == states);
    {
        state_log_mute mute;
        vfs_verify_result result;
        vfs_store_params store_params;
        store_params.blobs_dir = prod.blobs;
        store_params.install_dir = prod.install;
        TEST_CHECK(vfs_store_verify(plan, store_params, &result) == 0);
        TEST_CHECK(!result.corrupted);
        TEST_CHECK(result.files[0].sealed);
    }
    {
        CUopMappedFile pack;
        uo_data_init(fs_path_ascii(prod.install), 0, false);
        state_log_mute mute;
        TEST_CHECK(g_FileManager.UopLoadFile(pack, "pack0.uop", false));
        TEST_CHECK(pack.GetData(pack.GetAsset(prod.parts[0].uop_hash())) == prod.parts[0].payload);
        TEST_CHECK(pack.GetData(pack.GetAsset(prod.parts[1].uop_hash())) == prod.parts[1].payload);
        TEST_CHECK(pack.GetData(pack.GetAsset(prod.parts[2].uop_hash())) == prod.parts[2].payload);
    }
}

VFS_TEST(state_reconcile_missing_file)
{
    test_product_data prod;
    VfsPlan plan;
    TEST_CHECK(build_plan(prod, "state_missing", plan));

    std::vector<uint8_t> states;
    materialize(prod, plan, states);

    // the whole pack disappears (rm -rf accidents): every pack asset with
    // DB state fetched must downgrade, the loose files must survive
    fs_del(fs_path_join(prod.install, "pack0.uop"));
    TEST_CHECK(!fs_path_exists(fs_path_join(prod.install, "pack0.uop")));
    {
        state_log_mute mute;
        vfs_reconcile_stats stats;
        TEST_CHECK(vfs_state_reconcile(plan, prod.install, states, &stats));
        TEST_CHECK(stats.assets_repaired == 4); // A, B, C + the hole
    }
    TEST_CHECK(states[0] == vfs_pending);
    TEST_CHECK(states[4] == vfs_fetched);
    TEST_CHECK(states[5] == vfs_fetched);
}

// rewrites one pack entry exactly as the trap's stub patch does
// (Flags=0, CompressedSize = the stored span) and zeroes the stored head (the
// placeholder content): for stored (ct==0) assets the entry is byte-identical
// to the planned shape, which is the whole point
static void stub_patch_pack_entry(
    const test_product_data &prod,
    const VfsPlan &plan,
    size_t asset_index)
{
    const auto &file = plan.files[0];
    const auto &asset = file.assets[asset_index];
    const uint32_t stored = uint32_t(asset.ct == 0 ? asset.ul : asset.cl);
    const auto path = fs_path_join(prod.install, file.name);
    FILE *fp = fopen(fs_path_ascii(path), "r+b");
    TEST_CHECK_MSG(fp != nullptr, "could not reopen pack");
    UopFileEntry entry = {};
    entry.Offset = asset.file_offset;
    entry.MetadataSize = asset.ml;
    entry.CompressedSize = stored;
    entry.DecompressedSize = asset.ul;
    entry.Hash = asset.uop_hash;
    entry.Checksum = asset.mc;
    entry.Flags = 0;
    const uint64_t pos =
        sizeof(UopHeader) + sizeof(UopSection) + sizeof(UopFileEntry) * asset_index;
    TEST_CHECK(fseek(fp, long(pos), SEEK_SET) == 0);
    TEST_CHECK(fwrite(&entry, 1, sizeof(entry), fp) == sizeof(entry));
    std::vector<uint8_t> zeros(stored, 0); // the stub payload's zero tail
    TEST_CHECK(fseek(fp, long(asset.file_offset + asset.ml), SEEK_SET) == 0);
    TEST_CHECK(fwrite(zeros.data(), 1, zeros.size(), fp) == zeros.size());
    fclose(fp);
}

VFS_TEST(state_reconcile_never_promotes_stub_patches)
{
    // a stub patch must never become permanent through
    // reconcile. Stored (ct==0) patches are entry-identical to the plan, so
    // on-disk presence alone promotes neither a stubbed nor a pending asset;
    // only the heal path (a real block write, journalled 'F') promotes
    test_product_data prod;
    VfsPlan plan;
    TEST_CHECK(build_plan(prod, "state_stub_patch", plan));

    std::vector<uint8_t> states;
    materialize(prod, plan, states);

    // the trap's placeholder for part B (stored, ct==0)
    stub_patch_pack_entry(prod, plan, 1);
    const auto &part = plan.files[0].assets[1];

    // the entry shape alone still reports visible (the trap's boot mapping
    // relies on that), but the reconcile-grade check has the store root and
    // rejects the placeholder on content
    const auto path = fs_path_join(prod.install, plan.files[0].name);
    {
        size_t disk_size = 0;
        auto *disk = reinterpret_cast<uint8_t *>(fs_map(path, &disk_size));
        TEST_CHECK(disk != nullptr);
        TEST_CHECK(vfs_store_pack_asset_present(disk, uint64_t(disk_size), 1, part));
        TEST_CHECK(!vfs_store_pack_asset_present(disk, uint64_t(disk_size), 1, part, prod.blobs));
        fs_unmap(disk, disk_size);
    }

    states[1] = vfs_stubbed;
    {
        state_log_mute mute;
        vfs_reconcile_stats stats;
        TEST_CHECK(vfs_state_reconcile(plan, prod.install, states, &stats, prod.blobs));
        TEST_CHECK(stats.assets_upgraded == 0);
        TEST_CHECK(states[1] == vfs_stubbed); // disk presence never promotes
    }

    // the crash-before-the-'B'-journal window: pending + a stub-shaped entry
    // does not promote either (content, not shape, confirms real bytes)
    states[1] = vfs_pending;
    {
        state_log_mute mute;
        vfs_reconcile_stats stats;
        TEST_CHECK(vfs_state_reconcile(plan, prod.install, states, &stats, prod.blobs));
        TEST_CHECK(stats.assets_upgraded == 0);
        TEST_CHECK(states[1] == vfs_pending);
    }

    // the heal path is the only promotion: a real block write journals 'F'
    const std::vector<vfs_journal_record> records = { { 'F', 0, 1 } };
    vfs_state_apply_journal(plan, records, states);
    TEST_CHECK(states[1] == vfs_fetched);
    {
        state_log_mute mute;
        vfs_reconcile_stats stats;
        TEST_CHECK(vfs_state_reconcile(plan, prod.install, states, &stats, prod.blobs));
        TEST_CHECK(states[1] == vfs_fetched);
    }
}

VFS_TEST(state_reconcile_journal_recovery)
{
    // crash simulation: materialized bytes are on disk, but the state file is
    // gone; the journal replays the completion marks and reconcile confirms
    // them against the actual files
    test_product_data prod;
    VfsPlan plan;
    TEST_CHECK(build_plan(prod, "state_journal_recover", plan));

    std::vector<uint8_t> states;
    materialize(prod, plan, states);

    std::vector<uint8_t> lost(vfs_state_asset_count(plan), vfs_pending);
    std::vector<vfs_journal_record> records;
    TEST_CHECK(vfs_journal_replay(fs_path_join(prod.root, "journal.log"), records));
    TEST_CHECK(records.size() >= 6); // 5 fetched + 1 sealed
    vfs_state_apply_journal(plan, records, lost);
    {
        state_log_mute mute;
        vfs_reconcile_stats stats;
        TEST_CHECK(vfs_state_reconcile(plan, prod.install, lost, &stats));
        TEST_CHECK(stats.assets_repaired == 0);
    }
    TEST_CHECK(lost == states);
}

VFS_TEST(state_journal_rotation_after_clean_run)
{
    // a clean reconcile+save cycle folds every record into state.bin and then
    // rotates the journal: the log restarts empty and cannot grow unbounded
    test_product_data prod;
    VfsPlan plan;
    TEST_CHECK(build_plan(prod, "state_rotate", plan));

    vfs_params params;
    params.blobs_dir = prod.blobs;
    params.install_dir = prod.install;
    params.state_dir = prod.root; // the generator cleans these artifacts
    params.product_file = fs_path_join(prod.root, "prod.xml");
    vfs_set_params(params);

    const auto journal_path = fs_path_join(prod.root, "journal.log");
    vfs_materialize_stats stats;
    TEST_CHECK_MSG(vfs_materialize(&stats), "materialize failed");

    // the journal rotated to empty and state.bin kept every completion
    std::vector<vfs_journal_record> records;
    TEST_CHECK(vfs_journal_replay(journal_path, records));
    TEST_CHECK_MSG(records.empty(), "journal did not rotate after a clean run");
    std::vector<uint8_t> states;
    TEST_CHECK(
        vfs_state_load(fs_path_join(prod.root, "state.bin"), vfs_state_asset_count(plan), states));
    TEST_CHECK(states.size() == 6);
    for (auto state : states)
    {
        TEST_CHECK(state == vfs_fetched);
    }

    // the rotated journal keeps working: new marks append, and a fresh
    // process (all-pending DB + replayed records) still reconstructs them
    vfs_journal journal;
    TEST_CHECK(vfs_journal_open(journal, journal_path));
    const vfs_journal_record mark = { 'F', 1, 0 }; // tiledata.mul
    TEST_CHECK(vfs_journal_mark(journal, mark));
    TEST_CHECK(vfs_journal_close(journal));
    TEST_CHECK(vfs_journal_replay(journal_path, records));
    TEST_CHECK(records.size() == 1);
    TEST_CHECK(records[0].op == 'F');
    TEST_CHECK(records[0].file_index == 1);

    std::vector<uint8_t> fresh(vfs_state_asset_count(plan), vfs_pending);
    vfs_state_apply_journal(plan, records, fresh);
    TEST_CHECK(fresh[4] == vfs_fetched); // the first asset of file 1
    TEST_CHECK(fresh[0] == vfs_pending);
}

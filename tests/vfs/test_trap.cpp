// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// M3 trap tests: hole-paged managed mappings with fault-driven resolution
// (VUOI plan §6.5). No display, no network: the store is pre-seeded and the
// resolution backend runs offline (the game backend skips the fetch and
// materializes from the store), so every fault path is exercised
// deterministically:
//   (a) present-block reads return real bytes with zero faults
//   (b) a fault on a hole whose blob is in the store resolves on access
//   (c) a fault with the blob absent + offline degrades to a zero-filled
//       page (present bytes preserved), read succeeds, no crash
//   (d) a page straddling two blocks flips only when both are covered
//   (e) sealed files map plain readable: zero faults
//   (f) out-of-registry containment logic (no crash is attempted)
//   (g) concurrent faults on the same block resolve once (dedup)
//   (h) GetData pre-resolve serves bytes without triggering a fault
// plus the loose-file whole-fetch fallback (§10.3), and the M3 review
// regressions:
//   (i) unmap racing an in-flight resolution (m1: no UAF, resolver completes)
//   (j) write faults chain instead of resolving (M2: discriminator + fork)
//   (k) a zeroed page heals to real via ensure once its blob lands (m7)

#define LOGGER_MODULE TestVfs

#include <stdio.h>
#include <string.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#if !defined(_MSC_VER)
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ucontext.h>
#endif

#include "test_main.h"
#include "test_product.h"

#include <xuocore/common.h>
#include <common/logging/logging.h>
#include <xuocore/uodata.h>
#include <xuocore/uop.h>
#include <xuocore/vfs.h>
#include <xuocore/vfs_store.h>
#include <xuocore/vfs_trap.h>

// mutes the production logging of expected error paths (failed resolutions,
// load warnings) so the suite output stays deterministic
struct trap_log_mute
{
    eLogSystem saved;
    trap_log_mute()
        : saved(g_LogEnabled)
    {
        g_LogEnabled = LogSystemNone;
    }
    ~trap_log_mute() { g_LogEnabled = saved; }
};

// ---------------------------------------------------------------------------
// environment: product + streaming params + skeleton + partial materialize
// ---------------------------------------------------------------------------

struct trap_env
{
    test_product_data prod;
    VfsPlan plan;
    vfs_store_params store;
    size_t pack_file_index = 0;
    size_t loose_file_index = 0;
};

// default pack0.uop layout (page-computed below): [0] zlib ml=12 ul=4096,
// [1] stored ml=0 ul=777, [2] zlib ml=88 ul=1000, [3] hole; every block of
// the default product shares the first page with the entry table
static const size_t TRAP_PART_STORED = 1;

static bool trap_setup(trap_env &env, const char *name)
{
    if (!test_build_product(env.prod, name))
    {
        vfs_test_fail(__FILE__, __LINE__, "product build failed");
        return false;
    }

    vfs_params params;
    params.enabled = true;
    params.offline = true; // no fetch engine in these tests: store only
    params.blobs_dir = env.prod.blobs;
    params.install_dir = env.prod.install;
    params.state_dir = fs_path_join(env.prod.root, "state");
    params.product_file = fs_path_join(env.prod.root, "prod.xml");
    vfs_set_params(params);

    vfs_manifest_params manifest_params;
    manifest_params.product_file = params.product_file;
    manifest_params.blobs_dir = env.prod.blobs;
    if (!vfs_manifest_build(manifest_params, env.plan))
    {
        vfs_test_fail(__FILE__, __LINE__, "plan build failed");
        return false;
    }
    for (size_t i = 0; i < env.plan.files.size(); ++i)
    {
        if (env.plan.files[i].name == env.prod.pack_name)
            env.pack_file_index = i;
        if (env.plan.files[i].name == "tiledata.mul")
            env.loose_file_index = i;
    }
    env.store.blobs_dir = env.prod.blobs;
    env.store.install_dir = env.prod.install;
    env.store.state_dir = params.state_dir;
    if (!vfs_store_prepare(env.plan, env.store))
    {
        vfs_test_fail(__FILE__, __LINE__, "skeleton prepare failed");
        return false;
    }
    return true;
}

// custom layout for page-straddle coverage: block A spans two pages, block B
// shares A's last page (sizes derived from the system page size)
static const size_t TRAP_STRADDLE_A = 0;
static const size_t TRAP_STRADDLE_B = 1;

static bool trap_setup_straddle(trap_env &env, const char *name)
{
    const size_t page = vfs_trap_page_size();
    test_add_part(env.prod, 0x000001a1, 0x00000aa1, 0, 0, 2 * page);
    test_add_part(env.prod, 0x000001b2, 0x00000bb2, 0, 0, page);
    if (!trap_setup(env, name))
        return false;
    return true;
}

// materializes a subset of pack assets through the M1 write path
static bool trap_materialize(trap_env &env, const std::vector<uint32_t> &assets)
{
    std::vector<uint8_t> states;
    vfs_journal journal;
    std::vector<vfs_store_item> items;
    for (uint32_t ai : assets)
    {
        items.push_back({ uint32_t(env.pack_file_index), ai });
    }
    if (!vfs_journal_open(journal, fs_path_join(env.prod.root, "journal.log")))
        return false;
    vfs_materialize_stats stats;
    const bool ok = vfs_store_materialize(env.plan, env.store, states, &journal, &stats, &items);
    vfs_journal_close(journal);
    return ok;
}

// removes a blob from the store (404 / absent-blob exercises)
static bool trap_drop_blob(const trap_env &env, size_t part)
{
    const auto path = fs_path_join(env.prod.blobs, env.prod.parts[part].blob_rel);
    fs_del(path);
    return !fs_path_exists(path);
}

static const VfsAsset &trap_asset(const trap_env &env, size_t ai)
{
    return env.plan.files[env.pack_file_index].assets[ai];
}

static std::vector<uint8_t> trap_block_bytes(const trap_env &env, size_t ai)
{
    // the bytes the pack holds for the asset: [metadata][stored payload]
    const auto &asset = trap_asset(env, ai);
    std::vector<uint8_t> bytes = test_synth_metadata(asset.ml);
    const auto &payload =
        asset.ct == 0 ? env.prod.parts[ai].payload : env.prod.parts[ai].blob;
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}

// payload byte count the pack holds for the asset (blocks store the
// compressed blob for ct == 1)
static uint64_t trap_block_payload_size(const VfsAsset &asset)
{
    return asset.ct == 0 ? asset.ul : asset.cl;
}

static vfs_trap_stats trap_stats()
{
    vfs_trap_stats stats;
    vfs_trap_stats_get(&stats);
    return stats;
}

// ---------------------------------------------------------------------------
// (a) present blocks read with zero faults
// ---------------------------------------------------------------------------

VFS_TEST(trap_map_reads_present_block)
{
    trap_log_mute mute;
    trap_env env;
    TEST_CHECK(trap_setup_straddle(env, "trap_present"));

    // block A materialized before the map: page 0 (entry table + A's head)
    // is fully covered, so reads there cannot fault
    TEST_CHECK(trap_materialize(env, { TRAP_STRADDLE_A }));

    const auto path = fs_path_join(env.prod.install, env.prod.pack_name);
    uint8_t *base = nullptr;
    size_t size = 0;
    TEST_CHECK(vfs_map_file(path, base, size));
    TEST_CHECK(base != nullptr);
    TEST_CHECK(size == env.plan.files[env.pack_file_index].size);

    const auto &asset = trap_asset(env, TRAP_STRADDLE_A);
    const auto page = vfs_trap_page_size();
    const auto before = trap_stats();
    TEST_CHECK(vfs_trap_range_present(base, 0, size_t(page)));
    TEST_CHECK_MSG(
        memcmp(base + asset.file_offset, env.prod.parts[TRAP_STRADDLE_A].payload.data(), 64) == 0,
        "present bytes mismatch");

    UopHeader header;
    memcpy(&header, base, sizeof(header));
    TEST_CHECK(header.Magic == MYP_MAGIC);

    const auto after = trap_stats();
    TEST_CHECK_MSG(after.faults - before.faults == 0, "present pages must not fault");

    vfs_unmap_file(base, size);
}

// ---------------------------------------------------------------------------
// (b) a hole whose blob is in the store resolves on first access
// ---------------------------------------------------------------------------

VFS_TEST(trap_fault_resolves_from_store)
{
    trap_log_mute mute;
    trap_env env;
    TEST_CHECK(trap_setup(env, "trap_resolve"));

    // nothing materialized: every block is a hole, all blobs present
    const auto path = fs_path_join(env.prod.install, env.prod.pack_name);
    uint8_t *base = nullptr;
    size_t size = 0;
    TEST_CHECK(vfs_map_file(path, base, size));
    TEST_CHECK(base != nullptr);

    const auto &asset = trap_asset(env, TRAP_PART_STORED);
    const uint64_t at = asset.file_offset + asset.ml;
    TEST_CHECK_MSG(
        !vfs_trap_range_present(base, at, trap_block_payload_size(asset)),
        "hole must start protected");

    const auto before = trap_stats();
    const auto expected = trap_block_bytes(env, TRAP_PART_STORED);
    TEST_CHECK_MSG(
        memcmp(base + at, expected.data() + asset.ml, size_t(trap_block_payload_size(asset))) == 0,
        "resolved bytes mismatch");
    const auto after = trap_stats();
    TEST_CHECK_MSG(after.faults - before.faults >= 1, "expected at least one fault");
    TEST_CHECK_MSG(after.resolved - before.resolved >= 1, "expected the fault resolved");
    TEST_CHECK_MSG(after.zeroed - before.zeroed == 0, "nothing should zero-fill here");

    // the page is readable afterwards: re-reads do not fault
    TEST_CHECK(vfs_trap_range_present(base, at, trap_block_payload_size(asset)));
    volatile uint8_t probe = base[at];
    (void)probe;
    const auto settled = trap_stats();
    TEST_CHECK_MSG(settled.faults - after.faults == 0, "re-read must not fault");

    // the write path ran: the on-disk entry table now carries the block
    uint8_t *disk = nullptr;
    size_t disk_size = 0;
    disk = reinterpret_cast<uint8_t *>(fs_map(path, &disk_size));
    TEST_CHECK(disk != nullptr);
    TEST_CHECK(vfs_store_pack_asset_present(disk, uint64_t(disk_size), TRAP_PART_STORED, asset));
    fs_unmap(disk, disk_size);

    vfs_unmap_file(base, size);
}

// ---------------------------------------------------------------------------
// (c) hole with the blob absent + offline degrades to a zero-filled page
// ---------------------------------------------------------------------------

VFS_TEST(trap_fault_offline_zero_fills)
{
    trap_log_mute mute;
    trap_env env;
    TEST_CHECK(trap_setup(env, "trap_zero"));

    // the first (zlib) part's blob is gone; the stored part keeps its blob.
    // all blocks share the first page with the entry table, so the faulting
    // page resolves real bytes for the present blocks and zeros for the
    // missing one - present bytes are never clobbered
    TEST_CHECK(trap_drop_blob(env, 0));

    const auto path = fs_path_join(env.prod.install, env.prod.pack_name);
    uint8_t *base = nullptr;
    size_t size = 0;
    TEST_CHECK(vfs_map_file(path, base, size));

    const auto &gone = trap_asset(env, 0);
    const auto &kept = trap_asset(env, TRAP_PART_STORED);
    const uint64_t gone_at = gone.file_offset + gone.ml;
    const uint64_t kept_at = kept.file_offset + kept.ml;

    const auto before = trap_stats();
    const auto expected = trap_block_bytes(env, TRAP_PART_STORED);
    TEST_CHECK_MSG(
        memcmp(base + kept_at, expected.data() + kept.ml, kept.ul) == 0,
        "kept block must be real bytes");
    for (size_t i = 0; i < gone.cl; ++i)
    {
        if (base[gone_at + i] != 0)
        {
            vfs_test_fail(__FILE__, __LINE__, "missing block must read as zeros");
            return;
        }
    }
    const auto after = trap_stats();
    TEST_CHECK_MSG(after.faults - before.faults >= 1, "expected a fault");
    TEST_CHECK_MSG(after.zeroed - before.zeroed == 1, "expected one zero-filled page");
    TEST_CHECK(vfs_trap_page_mark_at(base, gone_at) == vfs_trap_mark_zeroed);
    TEST_CHECK(vfs_trap_page_mark_at(base, kept_at) == vfs_trap_mark_zeroed);

    // reads stay successful (no crash, no refault storm)
    TEST_CHECK(vfs_trap_range_present(base, gone_at, gone.cl));

    vfs_unmap_file(base, size);
}

// ---------------------------------------------------------------------------
// (d) a page straddling two blocks flips only when both are covered
// ---------------------------------------------------------------------------

VFS_TEST(trap_page_straddle_flips_when_covered)
{
    trap_log_mute mute;
    trap_env env;
    TEST_CHECK(trap_setup_straddle(env, "trap_straddle"));

    const auto &first = trap_asset(env, TRAP_STRADDLE_A);
    const auto &second = trap_asset(env, TRAP_STRADDLE_B);
    const auto page = vfs_trap_page_size();
    const uint64_t straddled = 2 * page; // page 2 holds A's tail and all of B

    // layout sanity: A spans pages, B lives inside A's last page
    TEST_CHECK(first.file_offset / page != (first.file_offset + first.ul - 1) / page);
    TEST_CHECK(second.file_offset / page == straddled / page);

    // only A is materialized; B's blob is added to the store afterwards
    // (simulates a blob arriving between boot and fault)
    TEST_CHECK(trap_materialize(env, { TRAP_STRADDLE_A }));
    TEST_CHECK(
        test_write_file(fs_path_join(env.prod.blobs, env.prod.parts[1].blob_rel),
                        env.prod.parts[1].payload));

    const auto path = fs_path_join(env.prod.install, env.prod.pack_name);
    uint8_t *base = nullptr;
    size_t size = 0;
    TEST_CHECK(vfs_map_file(path, base, size));

    // the straddled page must stay protected while B is absent, even though
    // A covers most of it
    TEST_CHECK_MSG(!vfs_trap_range_present(base, straddled, page), "straddled page protected");
    // A's head on page 0 is readable without a fault
    TEST_CHECK(
        memcmp(base + first.file_offset, env.prod.parts[0].payload.data(), 64) == 0);

    // first access to the straddled page resolves both covering blocks
    const auto before = trap_stats();
    TEST_CHECK_MSG(
        memcmp(
            base + second.file_offset,
            env.prod.parts[1].payload.data(),
            size_t(trap_block_payload_size(second))) == 0,
        "second block bytes mismatch");
    const auto after = trap_stats();
    TEST_CHECK_MSG(after.faults - before.faults >= 1, "expected a fault on the straddled page");
    TEST_CHECK(vfs_trap_range_present(base, straddled, page));
    TEST_CHECK(vfs_trap_page_mark_at(base, straddled) == vfs_trap_mark_real);
    // A's tail on the straddled page is its real bytes, not degradation zeros
    TEST_CHECK(
        memcmp(base + straddled,
               env.prod.parts[0].payload.data() + (straddled - first.file_offset),
               64) == 0);

    vfs_unmap_file(base, size);
}

// ---------------------------------------------------------------------------
// (e) sealed files map plain readable: zero faults
// ---------------------------------------------------------------------------

VFS_TEST(trap_sealed_maps_readable)
{
    trap_log_mute mute;
    trap_env env;
    TEST_CHECK(trap_setup(env, "trap_sealed"));

    TEST_CHECK(trap_materialize(env, { 0, 1, 2 }));

    const auto path = fs_path_join(env.prod.install, env.prod.pack_name);
    uint8_t *base = nullptr;
    size_t size = 0;
    TEST_CHECK(vfs_map_file(path, base, size));
    TEST_CHECK(base != nullptr);

    const auto before = trap_stats();
    for (size_t ai = 0; ai < 3; ++ai)
    {
        const auto &asset = trap_asset(env, ai);
        const uint64_t at = asset.file_offset + asset.ml;
        const auto expected = trap_block_bytes(env, ai);
        TEST_CHECK_MSG(
            memcmp(
                base + at,
                expected.data() + asset.ml,
                size_t(trap_block_payload_size(asset))) == 0,
            "sealed bytes mismatch at %zu",
            ai);
    }
    const auto after = trap_stats();
    TEST_CHECK_MSG(after.faults - before.faults == 0, "sealed files must not fault");
    TEST_CHECK(vfs_trap_page_mark_at(base, 0) == vfs_trap_mark_real);

    vfs_unmap_file(base, size);
}

// ---------------------------------------------------------------------------
// (f) registry containment: out-of-registry addresses are never ours
// ---------------------------------------------------------------------------

VFS_TEST(trap_registry_containment)
{
    trap_log_mute mute;
    trap_env env;
    TEST_CHECK(trap_setup(env, "trap_contain"));

    const auto path = fs_path_join(env.prod.install, env.prod.pack_name);
    uint8_t *base = nullptr;
    size_t size = 0;
    TEST_CHECK(vfs_map_file(path, base, size));
    TEST_CHECK(base != nullptr);

    // exact boundaries: [base, base + size) is ours, size and beyond is not
    TEST_CHECK(vfs_trap_contains(base, 0, 1));
    TEST_CHECK(vfs_trap_contains(base, size - 1, 1));
    TEST_CHECK_MSG(!vfs_trap_contains(base, size, 1), "end-of-file boundary is out of registry");
    TEST_CHECK_MSG(!vfs_trap_contains(base, size + 4096, 1), "beyond mapping is not ours");
    TEST_CHECK_MSG(!vfs_trap_contains(base, uint64_t(-1), 1), "wild offset is not ours");

    // wild pointers never resolve or report presence
    uint8_t *wild = reinterpret_cast<uint8_t *>(&size);
    TEST_CHECK(!vfs_trap_contains(wild, 0, 1));
    TEST_CHECK(!vfs_trap_range_present(wild, 0, 1));
    TEST_CHECK(vfs_trap_page_mark_at(wild, 0) == vfs_trap_mark_none);
    TEST_CHECK(!vfs_trap_resolve_range(wild, 0, 1));

    vfs_unmap_file(base, size);
    TEST_CHECK(!vfs_trap_contains(base, 0, 1)); // unregistered after unmap
}

// ---------------------------------------------------------------------------
// (g) concurrent faults on the same block resolve once (dedup)
// ---------------------------------------------------------------------------

struct trap_slow_backend
{
    std::atomic<int> calls{ 0 };
    trap_env *env = nullptr;

    static bool ensure(void *context, const vfs_trap_block *block)
    {
        auto *self = static_cast<trap_slow_backend *>(context);
        ++self->calls;
        // widen the race window so the second fault lands mid-resolution
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        std::vector<uint8_t> states;
        vfs_journal journal;
        vfs_store_params store;
        store.blobs_dir = self->env->prod.blobs;
        store.install_dir = self->env->prod.install;
        store.state_dir = fs_path_join(self->env->prod.root, "state");
        if (!vfs_journal_open(journal, fs_path_join(self->env->prod.root, "journal.log")))
            return false;
        const bool ok = vfs_store_rematerialize_asset(
            self->env->plan, store, states, &journal, block->file_index, block->asset_index);
        vfs_journal_close(journal);
        return ok;
    }
};

VFS_TEST(trap_concurrent_fault_dedup)
{
    trap_log_mute mute;
    trap_env env;
    // straddle layout: the faulting page is covered by exactly one absent
    // block (B), so "one resolution" is unambiguous
    TEST_CHECK(trap_setup_straddle(env, "trap_dedup"));
    TEST_CHECK(trap_materialize(env, { TRAP_STRADDLE_A }));

    const auto path = fs_path_join(env.prod.install, env.prod.pack_name);
    uint8_t *base = nullptr;
    size_t size = 0;
    TEST_CHECK(vfs_map_file(path, base, size));

    // hook the backend after the map: vfs_map_file (re)binds the game
    // backend, which would silently override the counting hook
    trap_slow_backend backend;
    backend.env = &env;
    vfs_trap_backend hooked;
    hooked.context = &backend;
    hooked.ensure = &trap_slow_backend::ensure;
    vfs_trap_set_backend(hooked);

    const auto &asset = trap_asset(env, TRAP_STRADDLE_B);
    const uint64_t at = asset.file_offset + asset.ml;
    const auto expected = trap_block_bytes(env, TRAP_STRADDLE_B);

    std::atomic<bool> reader_ok{ true };
    const auto reader = [&](std::chrono::milliseconds delay)
    {
        std::this_thread::sleep_for(delay);
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            if (memcmp(base + at, expected.data() + asset.ml, 32) == 0)
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        reader_ok = false;
    };
    std::thread first(reader, std::chrono::milliseconds(0));
    std::thread second(reader, std::chrono::milliseconds(30));
    first.join();
    second.join();

    const auto stats = trap_stats();
    printf(
        "  trap_concurrent_fault_dedup: calls=%d faults=%llu dedup_waits=%llu"
        " resolved=%llu zeroed=%llu\n",
        backend.calls.load(),
        (unsigned long long)stats.faults,
        (unsigned long long)stats.dedup_waits,
        (unsigned long long)stats.resolved,
        (unsigned long long)stats.zeroed);
    fflush(stdout);
    TEST_CHECK_MSG(reader_ok.load(), "readers did not see the resolved bytes");
    TEST_CHECK_MSG(backend.calls.load() == 1, "expected exactly one resolution");

    vfs_unmap_file(base, size);
}

// ---------------------------------------------------------------------------
// (h) GetData pre-resolve serves bytes without triggering a fault
// ---------------------------------------------------------------------------

VFS_TEST(trap_ensure_range_pre_resolve)
{
    trap_log_mute mute;
    trap_env env;
    TEST_CHECK(trap_setup(env, "trap_ensure"));

    // part 1's blob stays; parts 0/2 blobs are gone so nothing else can be
    // materialized eagerly when the entry-table page is resolved
    TEST_CHECK(trap_drop_blob(env, 0));
    TEST_CHECK(trap_drop_blob(env, 2));
    TEST_CHECK(trap_materialize(env, { TRAP_PART_STORED }));

    // production load path over the streaming pack (UopLoadFile unchanged):
    // mapping + table walk resolve the entry-table page once
    uo_data_init(fs_path_ascii(env.prod.install), 0, false);
    CUopMappedFile pack;
    {
        trap_log_mute load_mute;
        TEST_CHECK_MSG(g_FileManager.UopLoadFile(pack, "pack0.uop", false), "UopLoadFile failed");
    }

    const auto &asset = trap_asset(env, TRAP_PART_STORED);
    const auto *entry = pack.GetAsset(asset.uop_hash);
    TEST_CHECK_MSG(entry != nullptr, "materialized asset missing from the table");

    // GetData pre-resolve: no fault, correct bytes (the ensure may re-attempt
    // absent blocks sharing the range - that is the healing path - but the
    // access itself is served without a trap)
    const auto before = trap_stats();
    TEST_CHECK_MSG(
        pack.GetData(entry) == env.prod.parts[TRAP_PART_STORED].payload,
        "GetData payload mismatch");
    const auto after = trap_stats();
    TEST_CHECK_MSG(after.faults - before.faults == 0, "GetData must not fault");
    TEST_CHECK(vfs_trap_range_present(
        pack.Start, entry->Offset + entry->MetadataSize, entry->CompressedSize));

    // the absent zlib block: ensure degrades the range to readable zeros and
    // the existing DecompressBlock failure path returns an empty vector
    const auto &gone = trap_asset(env, 0);
    UopFileEntry hole;
    hole.Offset = gone.file_offset;
    hole.MetadataSize = gone.ml;
    hole.CompressedSize = uint32_t(gone.cl);
    hole.DecompressedSize = uint32_t(gone.ul);
    hole.Flags = 1;
    TEST_CHECK_MSG(pack.GetData(&hole).empty(), "zeros must fail zlib and return empty");
    const auto settled = trap_stats();
    TEST_CHECK_MSG(settled.faults - after.faults == 0, "ensure must serve without a fault");
    TEST_CHECK(vfs_trap_page_mark_at(pack.Start, gone.file_offset) == vfs_trap_mark_zeroed);

    pack.Unload();
}

// ---------------------------------------------------------------------------
// loose files stream whole-file on first fault (§10.3 MUL fallback)
// ---------------------------------------------------------------------------

VFS_TEST(trap_loose_file_whole_fetch)
{
    trap_log_mute mute;
    trap_env env;
    TEST_CHECK(trap_setup(env, "trap_loose"));

    const auto path = fs_path_join(env.prod.install, "tiledata.mul");
    uint8_t *base = nullptr;
    size_t size = 0;
    TEST_CHECK(vfs_map_file(path, base, size));
    TEST_CHECK(size == env.prod.loose[0].payload.size());

    const auto before = trap_stats();
    TEST_CHECK_MSG(memcmp(base, env.prod.loose[0].payload.data(), size) == 0,
                   "loose bytes mismatch");
    const auto after = trap_stats();
    TEST_CHECK_MSG(after.faults - before.faults >= 1, "expected a fault on the loose file");
    TEST_CHECK(vfs_trap_range_present(base, 0, size));

    vfs_unmap_file(base, size);
}

// ---------------------------------------------------------------------------
// (i) unmap racing an in-flight resolution: the resolver must complete (m1)
// ---------------------------------------------------------------------------

struct trap_unmap_backend
{
    std::atomic<int> calls{ 0 };
    std::atomic<bool> done{ false };

    static bool ensure(void *context, const vfs_trap_block *block)
    {
        (void)block;
        auto *self = static_cast<trap_unmap_backend *>(context);
        ++self->calls;
        // blocking backend: the unmap below lands mid-resolution and must
        // wait (or zombie) instead of unmapping under the resolver
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        self->done = true;
        return false; // blob absent: the resolution degrades to zero-fill
    }
};

VFS_TEST(trap_unmap_during_resolution)
{
    trap_log_mute mute;
    trap_env env;
    TEST_CHECK(trap_setup_straddle(env, "trap_unmap_race"));
    TEST_CHECK(trap_materialize(env, { TRAP_STRADDLE_A }));
    TEST_CHECK(trap_drop_blob(env, TRAP_STRADDLE_B)); // resolution cannot succeed

    const auto path = fs_path_join(env.prod.install, env.prod.pack_name);
    uint8_t *base = nullptr;
    size_t size = 0;
    TEST_CHECK(vfs_map_file(path, base, size));

    // hook the backend after the map (vfs_map_file rebinds the game backend)
    trap_unmap_backend backend;
    vfs_trap_backend hooked;
    hooked.context = &backend;
    hooked.ensure = &trap_unmap_backend::ensure;
    vfs_trap_set_backend(hooked);

    const auto &asset = trap_asset(env, TRAP_STRADDLE_B);
    const uint64_t at = asset.file_offset + asset.ml;

    std::atomic<int> reader_result{ -1 };
    std::thread reader([&] {
        const volatile uint8_t *p = base + at;
        reader_result = *p; // faults: the resolution runs under the slow backend
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // land mid-resolution
    const bool unmapped = vfs_trap_unmap(base);
    reader.join();

    // no crash reaching this point is the point: the unmap waited for the
    // resolver instead of unmapping/erasing under it
    TEST_CHECK_MSG(backend.done.load(), "the resolver must finish before unmap returns");
    TEST_CHECK_MSG(backend.calls.load() == 1, "expected exactly one backend call");
    TEST_CHECK_MSG(reader_result.load() == 0, "zero-filled page must read as zeros");
    TEST_CHECK_MSG(unmapped, "unmap must report the mapping handled");
    TEST_CHECK(!vfs_trap_contains(base, 0, 1)); // unregistered after unmap
}

// ---------------------------------------------------------------------------
// (j) write faults are never resolved: the discriminator declines writes and
// a real write to a flipped page chains to the default action (M2)
// ---------------------------------------------------------------------------

VFS_TEST(trap_write_fault_discriminator)
{
#if defined(_MSC_VER)
    EXCEPTION_RECORD record;
    memset(&record, 0, sizeof(record));
    record.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
    record.ExceptionInformation[0] = 0; // read
    TEST_CHECK(!vfs_trap_fault_is_write(&record));
    record.ExceptionInformation[0] = 1; // write
    TEST_CHECK(vfs_trap_fault_is_write(&record));
    TEST_CHECK(!vfs_trap_fault_is_write(nullptr));
#else
    ucontext_t read_ctx;
    ucontext_t write_ctx;
    memset(&read_ctx, 0, sizeof(read_ctx));
    memset(&write_ctx, 0, sizeof(write_ctx));
#if defined(REG_ERR)
    write_ctx.uc_mcontext.gregs[REG_ERR] = 2; // x86-64: the write bit
    TEST_CHECK_MSG(vfs_trap_fault_is_write(&write_ctx), "write context must be a write");
#endif
    // reads - and unknown contexts (zeroed, null) - must stay on the resolve
    // path: unknown platforms default to read on purpose
    TEST_CHECK(!vfs_trap_fault_is_write(&read_ctx));
    TEST_CHECK(!vfs_trap_fault_is_write(nullptr));
#endif
}

#if !defined(_MSC_VER)
VFS_TEST(trap_write_fault_chains)
{
    trap_log_mute mute;
    trap_env env;
    TEST_CHECK(trap_setup_straddle(env, "trap_write"));
    TEST_CHECK(trap_materialize(env, { TRAP_STRADDLE_A }));
    // B's blob stays in the store: the child's read resolves (flips the page
    // to PROT_READ), then its write must die with SIGSEGV

    const auto path = fs_path_join(env.prod.install, env.prod.pack_name);
    uint8_t *base = nullptr;
    size_t size = 0;
    TEST_CHECK(vfs_map_file(path, base, size));

    const auto &asset = trap_asset(env, TRAP_STRADDLE_B);
    const uint64_t at = asset.file_offset + asset.ml;
    fflush(nullptr);
    const pid_t pid = fork();
    TEST_CHECK(pid >= 0);
    if (pid == 0)
    {
        // child: the read resolves; the write to the flipped page is a game
        // bug - it must chain to the default action (SIGSEGV death), never
        // resolve (that would livelock) and never succeed silently
        alarm(20); // a livelocking handler still fails the test (SIGALRM)
        const auto expected = trap_block_bytes(env, TRAP_STRADDLE_B);
        if (memcmp(base + at, expected.data() + asset.ml, 32) != 0)
            _exit(64); // the read did not resolve: wrong failure mode
        volatile uint8_t *p = base + at;
        *p = 0xAB;
        _exit(0); // not reached while the chaining fix holds
    }
    int status = 0;
    waitpid(pid, &status, 0);
    TEST_CHECK_MSG(
        WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV,
        "a write to a flipped page must die with SIGSEGV");

    vfs_unmap_file(base, size);
}
#endif // !defined(_MSC_VER)

// ---------------------------------------------------------------------------
// (k) a zeroed page heals to real once its last absent block lands (m7)
// ---------------------------------------------------------------------------

VFS_TEST(trap_zeroed_page_heals)
{
    trap_log_mute mute;
    trap_env env;
    TEST_CHECK(trap_setup_straddle(env, "trap_heal"));
    TEST_CHECK(trap_materialize(env, { TRAP_STRADDLE_A }));
    TEST_CHECK(trap_drop_blob(env, TRAP_STRADDLE_B)); // the fault degrades to zeros

    const auto path = fs_path_join(env.prod.install, env.prod.pack_name);
    uint8_t *base = nullptr;
    size_t size = 0;
    TEST_CHECK(vfs_map_file(path, base, size));

    const auto &asset = trap_asset(env, TRAP_STRADDLE_B);
    const uint64_t at = asset.file_offset + asset.ml;
    volatile uint8_t probe = base[at]; // fault: zero-filled page
    (void)probe;
    TEST_CHECK_MSG(
        vfs_trap_page_mark_at(base, at) == vfs_trap_mark_zeroed, "degraded page must be zeroed");

    // the blob lands; the explicit ensure path (healing) re-runs resolution,
    // the last absent block marks present and the zeroed mark promotes to real
    TEST_CHECK(test_write_file(
        fs_path_join(env.prod.blobs, env.prod.parts[TRAP_STRADDLE_B].blob_rel),
        env.prod.parts[TRAP_STRADDLE_B].payload));
    TEST_CHECK(vfs_ensure_range(base, size_t(at), size_t(trap_block_payload_size(asset))));
    TEST_CHECK_MSG(
        vfs_trap_page_mark_at(base, at) == vfs_trap_mark_real, "healed page must be real");

    const auto expected = trap_block_bytes(env, TRAP_STRADDLE_B);
    TEST_CHECK_MSG(
        memcmp(base + at, expected.data() + asset.ml, size_t(trap_block_payload_size(asset))) == 0,
        "healed read must return real bytes");

    vfs_unmap_file(base, size);
}

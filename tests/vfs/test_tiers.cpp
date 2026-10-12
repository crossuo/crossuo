// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// Tier assignment (vfs_manifest_tier_of): the name tables must apply to both
// shapes of a file - the loose asset and the per-block pack assets the range
// refinement turns loose .uop files into. A refined file falling into the
// pack defaults loses its boot tier (soundlegacymul.uop's T1 ride-along
// degraded to sweep order), and the legacy anim mul set must converge last,
// not lead the plan-order sweep.

#define LOGGER_MODULE TestVfs

#include <string.h>

#include "test_main.h"

#include <xuocore/uop.h>
#include <xuocore/vfs_manifest.h>

static VfsPlan::VfsFile make_file(const char *name)
{
    VfsPlan::VfsFile file;
    file.name = name;
    return file;
}

static VfsAsset make_loose(const char *name)
{
    VfsAsset asset;
    asset.name = name; // loose: no uop_file
    return asset;
}

static VfsAsset make_block(const char *uop_file, uint64_t hash)
{
    VfsAsset asset;
    asset.uop_hash = hash;
    asset.uop_file = uop_file; // refined: a pack block of the file
    return asset;
}

VFS_TEST(tier_loose_name_tables)
{
    VfsPlan::VfsFile file;

    file = make_file("tiledata.mul");
    TEST_CHECK(vfs_manifest_tier_of(file, make_loose("tiledata.mul")) == vfs_tier_t0);
    file = make_file("artidx.mul");
    TEST_CHECK(vfs_manifest_tier_of(file, make_loose("artidx.mul")) == vfs_tier_t0);

    file = make_file("staidx0.mul");
    TEST_CHECK(vfs_manifest_tier_of(file, make_loose("staidx0.mul")) == vfs_tier_t1);
    file = make_file("unifont6.mul");
    TEST_CHECK(vfs_manifest_tier_of(file, make_loose("unifont6.mul")) == vfs_tier_t1);

    // mixed-case plan names resolve through the lowercased key
    file = make_file("TileData.MUL");
    TEST_CHECK(vfs_manifest_tier_of(file, make_loose("tiledata.mul")) == vfs_tier_t0);

    file = make_file("cliloc.enu");
    TEST_CHECK(vfs_manifest_tier_of(file, make_loose("cliloc.enu")) == vfs_tier_t2);

    file = make_file("music/dragons.mp3");
    TEST_CHECK(vfs_manifest_tier_of(file, make_loose("music/dragons.mp3")) == vfs_tier_t3);
}

VFS_TEST(tier_legacy_anim_muls_converge_last)
{
    VfsPlan::VfsFile file;
    // the >= 7.0 client never reads these (AnimationFrame*.uop replaces the
    // set): deferred to T3 so the sweep stops leading with ~730 MB of
    // legacy fallback
    for (const char *name : {"anim.mul", "anim2.mul", "anim3.mul", "anim4.mul", "anim5.mul", "anim6.mul"})
    {
        file = make_file(name);
        TEST_CHECK_MSG(
            vfs_manifest_tier_of(file, make_loose(name)) == vfs_tier_t3, name);
    }
    // the modern replacement and the legacy indexes keep their tiers
    file = make_file("animationframe3.uop");
    TEST_CHECK(
        vfs_manifest_tier_of(file, make_loose("animationframe3.uop")) == vfs_tier_t2);
    file = make_file("anim.idx");
    TEST_CHECK(vfs_manifest_tier_of(file, make_loose("anim.idx")) == vfs_tier_t1);
}

VFS_TEST(tier_refined_blocks_keep_the_file_tier)
{
    VfsPlan::VfsFile file;
    // soundlegacymul.uop rides along with Load (T1): refinement turns the
    // loose file into per-block pack assets, and those blocks must not fall
    // back to the T2 pack default
    file = make_file("soundlegacymul.uop");
    TEST_CHECK(
        vfs_manifest_tier_of(file, make_block("soundlegacymul.uop", 0x1234)) == vfs_tier_t1);

    // ordinary pack blocks stay on-demand
    file = make_file("artlegacymul.uop");
    TEST_CHECK(
        vfs_manifest_tier_of(file, make_block("artlegacymul.uop", 0x5678)) == vfs_tier_t2);

    // the MainMisc core set keeps its hash-keyed T0, other MainMisc blocks
    // stay T2
    file = make_file("MainMisc.uop");
    TEST_CHECK(vfs_manifest_tier_of(file, make_block("MainMisc.uop", Asset::Strings)) == vfs_tier_t0);
    TEST_CHECK(vfs_manifest_tier_of(file, make_block("MainMisc.uop", 0xDEAD)) == vfs_tier_t2);
}

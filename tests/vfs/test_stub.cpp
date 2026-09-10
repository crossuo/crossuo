// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// M4 stub tests: payload generators parsed at the format level (mirroring the
// game's real read loops - the readers live in src/ and drag GL/SDL deps, so
// the loops are mirrored here with pointers at the source), plus trap
// integration: fault -> stub bytes, healing -> real bytes + hot-swap refresh.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include "test_main.h"
#include "test_product.h"

#include <common/logging/logging.h>
#include <xuocore/common.h>
#include <xuocore/uodata.h>
#include <xuocore/uop.h>
#include <xuocore/vfs.h>
#include <xuocore/vfs_store.h>
#include <xuocore/vfs_stub.h>
#include <xuocore/vfs_trap.h>

namespace
{

struct stub_log_mute
{
    eLogSystem saved;
    stub_log_mute()
    {
        saved = g_LogEnabled;
        g_LogEnabled = eLogSystem::LogSystemNone;
    }
    ~stub_log_mute() { g_LogEnabled = saved; }
};

// little-endian readers for layout checks
uint16_t rd16(const uint8_t *p)
{
    return uint16_t(p[0] | (p[1] << 8));
}

uint32_t rd32(const uint8_t *p)
{
    return uint32_t(p[0]) | uint32_t(p[1] << 8) | uint32_t(p[2] << 16) | uint32_t(p[3] << 24);
}

// ---------------------------------------------------------------------------
// trap env: one pack whose kind is forced by the pack name, plus the standard
// loose files; mirrors tests/vfs/test_trap.cpp's trap_setup
// ---------------------------------------------------------------------------

struct stub_env
{
    test_product_data prod;
    VfsPlan plan;
    vfs_store_params store;
    size_t pack_file_index = 0;
};

// params/plan/skeleton plumbing shared by every stub test; assumes the
// product's parts/loose are registered. The stub subject is part 1: its blob
// is dropped (offline: nothing can fetch)
bool stub_setup_env(stub_env &env, const char *name)
{
    if (!test_build_product(env.prod, name))
    {
        vfs_test_fail(__FILE__, __LINE__, "product build failed");
        return false;
    }

    vfs_params params;
    params.enabled = true;
    params.offline = true; // store only; no fetch engine
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
    }
    env.store.blobs_dir = env.prod.blobs;
    env.store.install_dir = env.prod.install;
    env.store.state_dir = params.state_dir;
    if (!vfs_store_prepare(env.plan, env.store))
    {
        vfs_test_fail(__FILE__, __LINE__, "skeleton prepare failed");
        return false;
    }
    const auto blob = fs_path_join(env.prod.blobs, env.prod.parts[1].blob_rel);
    fs_del(blob);
    return !fs_path_exists(blob);
}

bool stub_setup(stub_env &env, const char *name, const char *pack_name)
{
    env.prod.pack_name = pack_name;
    // [0] stored (always kept), [1] zlib (the stub/heal subject)
    test_add_part(env.prod, 0x000002a1, 0x00001aa1, 0, 0, 1000);
    test_add_part(env.prod, 0x000002b2, 0x00001bb2, 1, 12, 2000);
    return stub_setup_env(env, name);
}

// sentinel "sprite" for the refresh callback: production passes the game's
// DestroySprite (src/CrossUO.cpp:547); the test records the drop
static int g_SentinelDestroyed = 0;

static void destroy_sentinel_sprite(CIndexObject *obj)
{
    obj->UserData = nullptr;
    g_SentinelDestroyed = 1;
}

} // namespace

// ---------------------------------------------------------------------------
// (a) generator round-trips, parsed at the format level
// ---------------------------------------------------------------------------

VFS_TEST(stub_kind_router)
{
    TEST_CHECK(vfs_stub_kind_for("art.mul") == vfs_stub_art);
    TEST_CHECK(vfs_stub_kind_for("artdif.mul") == vfs_stub_art);
    TEST_CHECK(vfs_stub_kind_for("artLegacyMUL.uop") == vfs_stub_art);
    TEST_CHECK(vfs_stub_kind_for("gumpart.mul") == vfs_stub_gump);
    TEST_CHECK(vfs_stub_kind_for("gumpartLegacyMUL.uop") == vfs_stub_gump);
    TEST_CHECK(vfs_stub_kind_for("map0.mul") == vfs_stub_map_block);
    TEST_CHECK(vfs_stub_kind_for("map0LegacyMUL.uop") == vfs_stub_map_block);
    TEST_CHECK(vfs_stub_kind_for("mapdif1.mul") == vfs_stub_map_block);
    TEST_CHECK(vfs_stub_kind_for("texmaps.mul") == vfs_stub_texture);
    TEST_CHECK(vfs_stub_kind_for("light.mul") == vfs_stub_light);
    TEST_CHECK(vfs_stub_kind_for("sound.mul") == vfs_stub_sound);
    TEST_CHECK(vfs_stub_kind_for("soundLegacyMUL.uop") == vfs_stub_sound);
    TEST_CHECK(vfs_stub_kind_for("Music/Digital/Britainpos.mp3") == vfs_stub_music);
    TEST_CHECK(vfs_stub_kind_for("anim.mul") == vfs_stub_anim);
    TEST_CHECK(vfs_stub_kind_for("AnimationFrame5.uop") == vfs_stub_anim);
    TEST_CHECK(vfs_stub_kind_for("AnimationSequence.uop") == vfs_stub_anim);
    TEST_CHECK(vfs_stub_kind_for("multi.mul") == vfs_stub_multi);
    TEST_CHECK(vfs_stub_kind_for("MultiCollection.uop") == vfs_stub_multi);
    TEST_CHECK(vfs_stub_kind_for("Multimap.rle") == vfs_stub_multimap);
    TEST_CHECK(vfs_stub_kind_for("speech.mul") == vfs_stub_speech);
    TEST_CHECK(vfs_stub_kind_for("Cliloc.exp") == vfs_stub_cliloc);
    TEST_CHECK(vfs_stub_kind_for("string_dictionary.uop") == vfs_stub_cliloc);
    // never-stubbed or unknown files land on zeros (they are T0 anyway)
    TEST_CHECK(vfs_stub_kind_for("statics0.mul") == vfs_stub_misc);
    TEST_CHECK(vfs_stub_kind_for("staidx0.mul") == vfs_stub_misc);
    TEST_CHECK(vfs_stub_kind_for("tiledata.mul") == vfs_stub_misc);
    TEST_CHECK(vfs_stub_kind_for("hues.mul") == vfs_stub_misc);
    TEST_CHECK(vfs_stub_kind_for("unifont6.mul") == vfs_stub_misc);
    TEST_CHECK(vfs_stub_kind_for("radarcol.mul") == vfs_stub_misc);
    TEST_CHECK(vfs_stub_kind_for("fonts.mul") == vfs_stub_misc);
    TEST_CHECK(vfs_stub_kind_for(nullptr) == vfs_stub_misc);
}

VFS_TEST(stub_gump_uop_parses_like_reader)
{
    // GetGumpPixels mirror (src/Managers/UOFileReader.cpp:15): UOP blocks
    // carry width/height in the first 8 bytes, then a u32 lookup per row and
    // GUMP_BLOCK runs. The stub dims MUST be 0x0: UopReadIndexFile bakes them
    // into CIndexObject, so an invented 1x1 would overflow a healed re-decode
    // (it writes W*H runs into a width*height pixel vector with no bounds
    // check) - 0x0 short-circuits the reader (blocksize == 0 => empty)
    const uint64_t ul = 64;
    uint8_t buf[64] = {};
    TEST_CHECK(vfs_stub_fill(vfs_stub_gump, ul, true, buf, sizeof(buf)) > 0);
    const int32_t width = int32_t(rd32(buf));
    const int32_t height = int32_t(rd32(buf + 4));
    TEST_CHECK(width == 0);
    TEST_CHECK(height == 0);
    TEST_CHECK(width * height == 0); // the reader's blocksize short-circuit
    const uint8_t *rows = buf + 8;   // Address skips the 8 width/height bytes
    const uint32_t lookup0 = rd32(rows);
    const uint32_t data_size = uint32_t(ul - 8);
    TEST_CHECK(lookup0 == data_size / 4); // row starts past the runs: zero work
    // the row loop parses exactly this many run blocks (0 => untouched pixels)
    const int g_size = int(data_size / 4) - int(lookup0);
    TEST_CHECK(g_size == 0);
}

VFS_TEST(stub_gump_mul_parses_like_reader)
{
    // MUL gumps: width/height come from the idx, the payload is the lookup
    const uint64_t ul = 256;
    uint8_t buf[256] = {};
    TEST_CHECK(vfs_stub_fill(vfs_stub_gump, ul, false, buf, sizeof(buf)) > 0);
    const uint32_t lookup0 = rd32(buf);
    TEST_CHECK(lookup0 == uint32_t(ul) / 4);
    // every row parses lookup[y+1]-lookup[y] blocks (0) except the last
    // (DataSize/4 - lookup[0] = 0): fully transparent for any real w/h
    const uint32_t entries = uint32_t(ul) / 4; // lookup entries the payload holds
    for (uint32_t y = 1; y + 1 < entries && y < 62; ++y)
    {
        const uint32_t a = rd32(buf + 4 * y), b = rd32(buf + 4 * (y + 1));
        TEST_CHECK(a == 0 && b == 0); // runs of zeros: nothing drawn
    }
    const int g_size = int(uint32_t(ul) / 4) - int(lookup0);
    TEST_CHECK(g_size == 0);
}

VFS_TEST(stub_art_run_1x1_transparent)
{
    // GetArtPixels run branch mirror (src/Managers/UOFileReader.cpp:144):
    // [4]=width [6]=height, lineOffsets, then {XOffs,Run} pairs; an empty
    // first run terminates the row loop with all pixels untouched
    const uint64_t ul = 4096;
    uint8_t buf[4096] = {};
    TEST_CHECK(vfs_stub_fill(vfs_stub_art, ul, true, buf, sizeof(buf)) > 0);
    TEST_CHECK(rd32(buf) == 0); // flags
    const uint16_t width = rd16(buf + 4);
    const uint16_t height = rd16(buf + 4 + 2);
    TEST_CHECK(width == 1);
    TEST_CHECK(height == 1);
    TEST_CHECK(width > 0 && width < 1024); // the reader's sanity bound
    TEST_CHECK(height > 0 && height * 2 <= 5120);

    // walk the rows exactly like the reader: lineOffsets[y] (u16 units from
    // dataStart), then {XOffs, Run} pairs; empty pair => end of row
    const uint8_t *data_start = buf + 8 + height * 2;
    const uint16_t line_offset = rd16(buf + 8);
    const uint16_t x_offs = rd16(data_start + line_offset * 2);
    const uint16_t run = rd16(data_start + line_offset * 2 + 2);
    TEST_CHECK(x_offs == 0);
    TEST_CHECK(run == 0); // loop ends immediately: no pixel written
}

VFS_TEST(stub_art_raw_overlap_is_near_zero)
{
    // the same pack serves raw land tiles: the raw reader consumes 1012 u16s
    // as pixels; the run header leaks exactly two 1s into that stream
    // (positions 2 and 3) - invisible specks, and AllBlack-style handling
    // keeps the rest transparent
    uint8_t buf[4096] = {};
    TEST_CHECK(vfs_stub_fill(vfs_stub_art, 4096, true, buf, sizeof(buf)) > 0);
    int nonzero = 0;
    for (size_t i = 0; i < 1012; ++i)
    {
        if (rd16(buf + 2 * i) != 0)
            ++nonzero;
    }
    TEST_CHECK(nonzero == 2);
}

VFS_TEST(stub_anim_empty_frames)
{
    // UopReadAnimationHeader mirror (xuocore/uodata.cpp:1308): a 40-byte
    // header; FrameCount == 0 must yield zero frames and the Offset must keep
    // the fixed-offset dimension read (frame 16 + palette 512 + info 8)
    // inside the block
    const uint64_t ul = 1024;
    uint8_t buf[1024] = {};
    TEST_CHECK(vfs_stub_fill(vfs_stub_anim, ul, true, buf, sizeof(buf)) > 0);
    TEST_CHECK(rd32(buf + 32) == 0); // FrameCount
    TEST_CHECK(rd32(buf + 28) == 40); // HeaderSize
    TEST_CHECK(rd32(buf + 36) == 40); // Offset
    // the dimension path reads [Offset, Offset+16) then +512 then +8
    TEST_CHECK(40 + 16 + 512 + 8 <= ul);
    // UopReadAnimationFramesData: reserve(FrameCount) => zero frame reads
    TEST_CHECK(rd32(buf + 32) * 16 == 0);
}

VFS_TEST(stub_anim_squeezed_layout)
{
    // payloads between 536 and 575 bytes squeeze Offset to 0 so the fixed
    // layout still ends inside the block
    const uint64_t ul = 540;
    uint8_t buf[540] = {};
    TEST_CHECK(vfs_stub_fill(vfs_stub_anim, ul, true, buf, sizeof(buf)) > 0);
    TEST_CHECK(rd32(buf + 32) == 0);  // FrameCount
    TEST_CHECK(rd32(buf + 36) == 0);  // Offset squeezed to 0
    TEST_CHECK(16 + 512 + 8 <= ul);   // frame + palette + info fit
}

VFS_TEST(stub_anim_tiny_degrades_to_zeros)
{
    uint8_t buf[100] = {};
    TEST_CHECK(vfs_stub_fill(vfs_stub_anim, 100, true, buf, sizeof(buf)) == 0);
    for (size_t i = 0; i < sizeof(buf); ++i)
    {
        if (buf[i] != 0)
        {
            vfs_test_fail(__FILE__, __LINE__, "short anim payload must stay zeros");
            return;
        }
    }
}

VFS_TEST(stub_map_block_flat_tile)
{
    const uint64_t block_size = 196; // mulstruct.h MAP_BLOCK
    const uint64_t blocks = 64;      // one 64x64 map block
    const uint64_t ul = block_size * blocks;
    const size_t total = size_t(ul);
    std::vector<uint8_t> buf(total);
    TEST_CHECK(vfs_stub_fill(vfs_stub_map_block, ul, true, buf.data(), buf.size()) > 0);
    for (uint64_t b = 0; b < blocks; ++b)
    {
        const uint8_t *block = buf.data() + b * block_size;
        TEST_CHECK(rd32(block) == 0); // MAP_BLOCK::Header
        for (uint64_t c = 0; c < 64; ++c)
        {
            const uint8_t *cell = block + 4 + 3 * c;
            TEST_CHECK(rd16(cell) == VFS_STUB_MAP_FLAT_TILE_ID);
            TEST_CHECK(int8_t(cell[2]) == 0); // altitude 0
        }
    }
    // slice continuity: an arbitrary window matches the whole-payload fill
    uint8_t whole[196 * 2];
    uint8_t slice[37];
    TEST_CHECK(vfs_stub_fill(vfs_stub_map_block, ul, true, whole, sizeof(whole)) > 0);
    TEST_CHECK(vfs_stub_fill_slice(vfs_stub_map_block, ul, true, 196 + 10, slice, sizeof(slice)) > 0);
    TEST_CHECK(memcmp(slice, whole + 196 + 10, sizeof(slice)) == 0);
}

VFS_TEST(stub_multi_multimap_headers)
{
    // MultiCollection loop reads id + Count from the first 8 bytes; both are
    // zeros, so the fill reports no nonzero bytes but still clears the buffer
    uint8_t multi[64];
    memset(multi, 0xAB, sizeof(multi));
    TEST_CHECK(vfs_stub_fill(vfs_stub_multi, 64, true, multi, sizeof(multi)) == 0);
    for (size_t i = 0; i < sizeof(multi); ++i)
    {
        if (multi[i] != 0)
        {
            vfs_test_fail(__FILE__, __LINE__, "empty multi must be zeros");
            return;
        }
    }
    TEST_CHECK(rd32(multi) == 0);
    TEST_CHECK(rd32(multi + 4) == 0); // Count = 0: empty multi

    // MultiMap.cpp reads i32 bounds then an RLE body; 1x1 bounds + zeros body
    uint8_t mmap[100] = {};
    TEST_CHECK(vfs_stub_fill(vfs_stub_multimap, 100, false, mmap, sizeof(mmap)) > 0);
    TEST_CHECK(int32_t(rd32(mmap)) == 1);
    TEST_CHECK(int32_t(rd32(mmap + 4)) == 1);
    for (size_t i = 8; i < sizeof(mmap); ++i)
    {
        if (mmap[i] != 0)
        {
            vfs_test_fail(__FILE__, __LINE__, "multimap body must be zeros");
            return;
        }
    }
}

VFS_TEST(stub_zero_content_kinds_and_bounds)
{
    // texture/light/land/sound/music/misc/statics/cliloc/speech: pure zeros
    // (samples, pixels, tables); the readers' own bounds do the rest
    const vfs_stub_kind kinds[] = {
        vfs_stub_texture, vfs_stub_light, vfs_stub_land,     vfs_stub_sound,
        vfs_stub_music,   vfs_stub_misc,  vfs_stub_cliloc,   vfs_stub_speech,
    };
    uint8_t buf[128] = {};
    for (vfs_stub_kind kind : kinds)
    {
        memset(buf, 0xAB, sizeof(buf));
        TEST_CHECK(vfs_stub_fill(kind, 128, true, buf, sizeof(buf)) == 0);
        for (size_t i = 0; i < sizeof(buf); ++i)
        {
            if (buf[i] != 0)
            {
                vfs_test_fail(__FILE__, __LINE__, "zero-content kind must read as zeros");
                return;
            }
        }
    }
    // bounds: ul == 0, skips past the end, clamping, no overshoot
    TEST_CHECK(vfs_stub_fill(vfs_stub_gump, 0, true, buf, sizeof(buf)) == 0);
    TEST_CHECK(vfs_stub_fill_slice(vfs_stub_gump, 64, true, 64, buf, 8) == 0);
    TEST_CHECK(vfs_stub_fill_slice(vfs_stub_gump, 64, true, 60, buf, 8) == 0);
    TEST_CHECK(vfs_stub_fill_slice(vfs_stub_gump, 64, true, 0, nullptr, 8) == 0);
    memset(buf, 0xAB, sizeof(buf));
    vfs_stub_fill_slice(vfs_stub_art, 64, true, 0, buf, 8); // header clamped at ul
    TEST_CHECK(rd16(buf + 4) == 1); // width written (fits)
    TEST_CHECK(buf[8] == 0xAB); // nothing beyond ul
}

// ---------------------------------------------------------------------------
// (b) trap integration: faults serve stubs, healing promotes + refreshes
// ---------------------------------------------------------------------------

VFS_TEST(stub_pack_fault_serves_stub_bytes)
{
    stub_log_mute mute;
    stub_env env;
    TEST_CHECK(stub_setup(env, "stub_gump", "gumpartLegacyMUL.uop")); // kind: gump

    const auto path = fs_path_join(env.prod.install, env.prod.pack_name);
    uint8_t *base = nullptr;
    size_t size = 0;
    TEST_CHECK(vfs_map_file(path, base, size));

    const auto &gone = env.plan.files[env.pack_file_index].assets[1];
    const auto &kept = env.plan.files[env.pack_file_index].assets[0];
    const uint64_t gone_payload = gone.file_offset + gone.ml;
    const uint64_t kept_payload = kept.file_offset + kept.ml;

    // kept part: real bytes (present blob materialized at prepare); this read
    // also triggers the fault that stubs the gone part (shared first page)
    const auto stubs_before = vfs_stub_count();
    const auto &kept_bytes = env.prod.parts[0].payload;
    TEST_CHECK_MSG(
        memcmp(base + kept_payload, kept_bytes.data(), kept_bytes.size()) == 0,
        "kept block must be real bytes");

    // gone part: the fault serves the gump stub within the stored span
    const uint64_t capacity = gone.ct == 0 ? uint64_t(gone.ul) : uint64_t(gone.cl);
    TEST_CHECK(capacity >= vfs_stub_min_payload(vfs_stub_gump));
    const size_t span = size_t(capacity);
    std::vector<uint8_t> expected(span);
    TEST_CHECK(vfs_stub_fill(vfs_stub_gump, gone.ul, true, expected.data(), expected.size()) > 0);
    TEST_CHECK_MSG(
        memcmp(base + gone_payload, expected.data(), expected.size()) == 0,
        "stub payload mismatch");
    // page marks are page-granular: the page now carries the stub (the kept
    // block's real bytes were verified above on the same page)
    TEST_CHECK(vfs_trap_page_mark_at(base, gone_payload) == vfs_trap_mark_stubbed);
    TEST_CHECK(vfs_stub_count() == stubs_before + 1);

    // the on-disk entry advertises the stub as a stored block of the stub
    // size, but keeps the plan's hash/decompressed size: still "absent"
    uint8_t *disk = reinterpret_cast<uint8_t *>(fs_map(path, &size));
    TEST_CHECK(disk != nullptr);
    const uint64_t pos =
        sizeof(UopHeader) + sizeof(UopSection) + sizeof(UopFileEntry) * 1;
    const UopFileEntry *entry = reinterpret_cast<const UopFileEntry *>(disk + pos);
    TEST_CHECK(entry->Offset == gone.file_offset);
    TEST_CHECK(entry->Flags == 0);
    TEST_CHECK(entry->CompressedSize == uint32_t(capacity));
    TEST_CHECK(entry->DecompressedSize == uint32_t(gone.ul));
    TEST_CHECK(entry->Hash == gone.uop_hash);
    fs_unmap(disk, size);

    vfs_unmap_file(base, size);
}

VFS_TEST(stub_heal_promotes_and_refreshes)
{
    stub_log_mute mute;
    stub_env env;
    TEST_CHECK(stub_setup(env, "stub_heal", "artLegacyMUL.uop")); // kind: art

    const auto path = fs_path_join(env.prod.install, env.prod.pack_name);
    uint8_t *base = nullptr;
    size_t size = 0;
    TEST_CHECK(vfs_map_file(path, base, size));

    const auto &gone = env.plan.files[env.pack_file_index].assets[1];
    const uint64_t at = gone.file_offset + gone.ml;
    const uint64_t capacity = uint64_t(gone.cl);


    volatile uint8_t probe = base[at]; // fault: stub served
    (void)probe;
    TEST_CHECK(vfs_trap_page_mark_at(base, at) == vfs_trap_mark_stubbed);

    // not healed yet: refresh is a no-op and the flag survives
    TEST_CHECK(!vfs_refresh_block(base + at));

    // the blob lands; an explicit ensure (any pre-resolve) heals the asset
    const auto blob = fs_path_join(env.prod.blobs, env.prod.parts[1].blob_rel);
    TEST_CHECK(test_write_file(blob, env.prod.parts[1].blob));
    TEST_CHECK(vfs_ensure_range(base, size_t(at), size_t(capacity)));
    TEST_CHECK_MSG(vfs_trap_page_mark_at(base, at) == vfs_trap_mark_real, "heal must promote");

    // ct=1 blocks store the compressed blob: the mapping shows the stored
    // bytes (decompression happens in the readers)
    const auto &stored = env.prod.parts[1].blob;
    TEST_CHECK_MSG(
        memcmp(base + at, stored.data(), stored.size()) == 0, "healed bytes must be real");

    // the hot-swap refresh: an index object built over the stub reloads
    TEST_CHECK(vfs_refresh_block(base + at));
    CIndexObject obj;
    obj.Address = uintptr_t(base + at);
    obj.Stubbed = 1;
    obj.UserData = &g_SentinelDestroyed;
    TEST_CHECK(vfs_refresh_index(&obj, destroy_sentinel_sprite));
    TEST_CHECK(obj.Stubbed == 0);
    TEST_CHECK(obj.UserData == nullptr);
    TEST_CHECK(g_SentinelDestroyed);
    // with the flag cleared, refreshing again is a no-op
    TEST_CHECK(!vfs_refresh_index(&obj, destroy_sentinel_sprite));

    // the on-disk entry is back to the plan's shape (Flags=1, full cl)
    uint8_t *disk = reinterpret_cast<uint8_t *>(fs_map(path, &size));
    TEST_CHECK(disk != nullptr);
    TEST_CHECK(vfs_store_pack_asset_present(disk, uint64_t(size), 1, gone));
    fs_unmap(disk, size);

    vfs_unmap_file(base, size);
}

VFS_TEST(stub_healed_query_semantics)
{
    // M1 review: the trap's healed query is the animation hot-swap trigger,
    // so it must answer on the trap's own marks (stub-served + bytes landed),
    // never on the state DB byte - the heal itself flips that byte to
    // fetched, which made the old stubbed-AND-present check dead code
    stub_log_mute mute;
    stub_env env;
    TEST_CHECK(stub_setup(env, "stub_healed_q", "artLegacyMUL.uop")); // kind: art

    const auto path = fs_path_join(env.prod.install, env.prod.pack_name);
    uint8_t *base = nullptr;
    size_t size = 0;
    TEST_CHECK(vfs_map_file(path, base, size));

    const auto &gone = env.plan.files[env.pack_file_index].assets[1];
    const uint64_t at = gone.file_offset + gone.ml;

    TEST_CHECK(!vfs_trap_asset_healed(base + at)); // absent: no marks at all
    volatile uint8_t probe = base[at];             // fault: the stub is served
    (void)probe;
    TEST_CHECK(vfs_trap_page_mark_at(base, at) == vfs_trap_mark_stubbed);
    TEST_CHECK(!vfs_trap_asset_healed(base + at)); // stubbed, not healed yet

    const auto blob = fs_path_join(env.prod.blobs, env.prod.parts[1].blob_rel);
    TEST_CHECK(test_write_file(blob, env.prod.parts[1].blob));
    TEST_CHECK(vfs_ensure_range(base, size_t(at), size_t(gone.cl)));
    TEST_CHECK(vfs_trap_asset_healed(base + at)); // marker + healed => true

    vfs_unmap_file(base, size);

    // fresh mapping of the healed file: the asset was present (and real) at
    // map time, so no stub marker exists this session => false again
    uint8_t *base2 = nullptr;
    size_t size2 = 0;
    TEST_CHECK(vfs_map_file(path, base2, size2));
    TEST_CHECK(!vfs_trap_asset_healed(base2 + at));

    vfs_unmap_file(base2, size2);
}

VFS_TEST(stub_zerolat_offline_serves_instantly_and_heals)
{
    stub_log_mute mute;
    stub_env env;
    TEST_CHECK(stub_setup(env, "stub_zerolat", "soundLegacyMUL.uop"));
    // enable zero-latency for this test only (vfs_set_params binds the flag)
    vfs_params params = vfs_get_params();
    params.zerolat = true;
    vfs_set_params(params);

    const auto path = fs_path_join(env.prod.install, env.prod.pack_name);
    uint8_t *base = nullptr;
    size_t size = 0;
    TEST_CHECK(vfs_map_file(path, base, size));

    const auto &gone = env.plan.files[env.pack_file_index].assets[1];
    const uint64_t at = gone.file_offset + gone.ml;


    // no fetch engine, offline, zerolat: the fault must not wait - the page
    // flips readable immediately (with a sound stub: silence => zeroed mark,
    // no stub payload bytes for this kind)
    const auto before = vfs_stub_count();
    volatile uint8_t probe = base[at];
    (void)probe;
    TEST_CHECK(vfs_trap_page_mark_at(base, at) == vfs_trap_mark_zeroed);
    TEST_CHECK(vfs_stub_count() >= before);
    TEST_CHECK(!vfs_refresh_block(base + at)); // nothing healed yet

    // the blobs land in the store; healing still works offline. The whole
    // file is resolved so every absent block of the shared first page lands
    // (pages promote only when fully covered) and part0 materializes too
    const auto blob = fs_path_join(env.prod.blobs, env.prod.parts[1].blob_rel);
    TEST_CHECK(test_write_file(blob, env.prod.parts[1].blob));
    const bool ensured = vfs_ensure_range(base, 0, size);
    TEST_CHECK(ensured);
    TEST_CHECK(vfs_trap_page_mark_at(base, at) == vfs_trap_mark_real);
    TEST_CHECK(vfs_refresh_block(base + at));

    vfs_unmap_file(base, size);
}

VFS_TEST(stub_misc_and_loose_serve_zero_pages)
{
    stub_log_mute mute;
    stub_env env;
    TEST_CHECK(stub_setup(env, "stub_misc", "pack0.uop")); // unknown name: misc

    const auto path = fs_path_join(env.prod.install, env.prod.pack_name);
    uint8_t *base = nullptr;
    size_t size = 0;
    TEST_CHECK(vfs_map_file(path, base, size));

    const auto &gone = env.plan.files[env.pack_file_index].assets[1];
    const uint64_t at = gone.file_offset + gone.ml;
    volatile uint8_t probe = base[at];
    (void)probe;
    // misc has no payload content: plain zeros (M3 degradation), zeroed mark
    for (uint64_t i = 0; i < uint64_t(gone.cl); ++i)
    {
        if (base[at + i] != 0)
        {
            vfs_test_fail(__FILE__, __LINE__, "misc stub must read as zeros");
            return;
        }
    }
    TEST_CHECK(vfs_trap_page_mark_at(base, at) == vfs_trap_mark_zeroed);

    vfs_unmap_file(base, size);
}

// ---------------------------------------------------------------------------
// (c) C1 regression: the healed gump re-decodes with the real dims
// ---------------------------------------------------------------------------

// the crafted real gump behind the stub subject: the first 8 bytes are the
// dims UopReadIndexFile bakes (then skips), then a u32 lookup per row and one
// full-width GUMP_BLOCK run per row
static const int32_t GUMP_W = 64;
static const int32_t GUMP_H = 48;

static uint16_t gump_row_value(int32_t y)
{
    return uint16_t(0x4000 + y);
}

static std::vector<uint8_t> test_gump_payload()
{
    std::vector<uint8_t> p(size_t(8 + 4 * GUMP_H + 4 * GUMP_H), 0);
    const auto put32 = [&p](size_t at, uint32_t v) {
        p[at] = uint8_t(v);
        p[at + 1] = uint8_t(v >> 8);
        p[at + 2] = uint8_t(v >> 16);
        p[at + 3] = uint8_t(v >> 24);
    };
    put32(0, uint32_t(GUMP_W));
    put32(4, uint32_t(GUMP_H));
    for (int32_t y = 0; y < GUMP_H; ++y)
    {
        put32(size_t(8 + 4 * y), uint32_t(GUMP_H + y)); // lookup[y]: one block per row
        const size_t run_at = size_t(8 + 4 * GUMP_H + 4 * y);
        const uint16_t val = gump_row_value(y);
        p[run_at] = uint8_t(val);
        p[run_at + 1] = uint8_t(val >> 8);
        p[run_at + 2] = uint8_t(GUMP_W); // Run: the full row width
        p[run_at + 3] = uint8_t(GUMP_W >> 8);
    }
    return p;
}

VFS_TEST(stub_gump_heal_re_reads_baked_dims)
{
    stub_log_mute mute;
    stub_env env;
    env.prod.pack_name = "gumpartLegacyMUL.uop"; // kind: gump
    // [0] stored (kept); [1] the gump subject, stored (ct=0) so the healed
    // blob lands verbatim and the block start is the dims header
    test_add_part(env.prod, 0x000002a1, 0x00001aa1, 0, 0, 1000);
    test_add_part(env.prod, 0x000002b2, 0x00001bb2, 0, 0, 0);
    {
        auto &part = env.prod.parts[1];
        part.payload = test_gump_payload();
        part.blob = part.payload;
    }
    TEST_CHECK(stub_setup_env(env, "stub_gump_dims"));

    const auto path = fs_path_join(env.prod.install, env.prod.pack_name);
    uint8_t *base = nullptr;
    size_t size = 0;
    TEST_CHECK(vfs_map_file(path, base, size));

    const auto &gone = env.plan.files[env.pack_file_index].assets[1];
    const uint64_t at = gone.file_offset + gone.ml;
    TEST_CHECK(gone.ul == 8 + 4 * GUMP_H + 4 * GUMP_H);

    volatile uint8_t probe = base[at]; // fault: the stub is served
    (void)probe;
    TEST_CHECK(vfs_trap_page_mark_at(base, at) == vfs_trap_mark_stubbed);
    TEST_CHECK(vfs_trap_stub_kind_at(base + at) == vfs_stub_gump);

    // the dims an index build bakes over the stub: 0x0 (the C1 fix; a 1x1
    // here turned a healed re-decode into an unbounded run write)
    TEST_CHECK(rd32(base + at) == 0 && rd32(base + at + 4) == 0);

    // the real blob lands; the heal promotes the asset
    const auto blob = fs_path_join(env.prod.blobs, env.prod.parts[1].blob_rel);
    TEST_CHECK(test_write_file(blob, env.prod.parts[1].blob));
    TEST_CHECK(vfs_ensure_range(base, size_t(at), size_t(gone.ul)));
    TEST_CHECK(vfs_trap_page_mark_at(base, at) == vfs_trap_mark_real);
    TEST_CHECK_MSG(
        rd32(base + at) == uint32_t(GUMP_W) && rd32(base + at + 4) == uint32_t(GUMP_H),
        "healed block must start with the real dims");

    // an index object exactly as UopReadIndexFile built it over the stub
    CIndexObject io;
    static UopFileEntry block = {};
    io.Address = uintptr_t(base + at + 8); // the += 8 UopReadIndexFile applies
    io.DataSize = uint32_t(gone.ul - 8);
    io.UopBlock = &block;
    io.Stubbed = 1;
    io.UserData = &g_SentinelDestroyed;
    g_SentinelDestroyed = 0;
    TEST_CHECK(vfs_refresh_index(&io, destroy_sentinel_sprite));
    TEST_CHECK(io.Stubbed == 0);
    TEST_CHECK(g_SentinelDestroyed);
    TEST_CHECK_MSG(io.Width == GUMP_W && io.Height == GUMP_H, "dims must be re-read");

    // GetGumpPixels mirror under a canary: the reader has no bounds check,
    // so the loop must produce exactly W*H pixels and never touch the tail
    const size_t blocksize = size_t(io.Width) * size_t(io.Height);
    TEST_CHECK(blocksize != 0); // the reader's short-circuit would warn here
    std::vector<uint16_t> pixels(blocksize + 16, 0xDEAD);
    const uint8_t *data_start = reinterpret_cast<const uint8_t *>(io.Address);
    const uint32_t *lookup_list = reinterpret_cast<const uint32_t *>(data_start);
    bool oob = false;
    size_t highest = 0;
    for (int32_t y = 0; y < io.Height && !oob; ++y)
    {
        int g_size = 0;
        if (y < io.Height - 1)
        {
            g_size = int(lookup_list[y + 1] - lookup_list[y]);
        }
        else
        {
            g_size = int(io.DataSize / 4) - int(lookup_list[y]);
        }
        const auto *gmul = reinterpret_cast<const GUMP_BLOCK *>(data_start + lookup_list[y] * 4);
        int pos = y * io.Width;
        for (int i = 0; i < g_size && !oob; ++i)
        {
            const uint16_t val = gmul[i].Value;
            const uint16_t a = uint16_t((val != 0u ? 0x8000 : 0) | val);
            const int count = gmul[i].Run;
            for (int j = 0; j < count; ++j)
            {
                if (pos < 0 || size_t(pos) >= blocksize)
                {
                    oob = true; // the C1 overflow the 1x1 stub baked
                    break;
                }
                pixels[size_t(pos)] = a;
                ++pos;
            }
            if (size_t(pos) > highest)
                highest = size_t(pos);
        }
    }
    TEST_CHECK(!oob);
    TEST_CHECK(highest == blocksize); // exactly W*H pixels written
    for (size_t i = 0; i < 16; ++i)
    {
        if (pixels[blocksize + i] != 0xDEAD)
        {
            vfs_test_fail(__FILE__, __LINE__, "canary clobbered");
            return;
        }
    }
    TEST_CHECK(pixels[0] == uint16_t(0x8000u | gump_row_value(0)));
    TEST_CHECK(pixels[blocksize - 1] == uint16_t(0x8000u | gump_row_value(GUMP_H - 1)));

    vfs_unmap_file(base, size);
}

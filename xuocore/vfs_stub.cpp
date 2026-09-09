// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_stub: placeholder asset providers (VUOI plan §6.7, §8). See the header
// for the payload model; the per-kind layouts mirror the game's real parse
// code paths:
//   art run reader   src/Managers/UOFileReader.cpp GetArtPixels (run branch)
//   art raw reader   GetArtPixels (raw branch; land tiles of the same pack)
//   gump reader      GetGumpPixels (lookup + GUMP_BLOCK rows); UOP blocks
//                    carry width/height in their first 8 bytes, which
//                    UopReadIndexFile bakes into CIndexObject - the stub
//                    writes 0x0 there (readers short-circuit) and the
//                    hot-swap refresh re-reads the real dims (xuocore/vfs.cpp
//                    vfs_refresh_index, xuocore/uodata.cpp UopReadIndexFile)
//   anim reader      xuocore/uodata.cpp UopReadAnimationHeader/-Frames(Data)
//   multi reader     uodata.cpp CIndexMulti::ReadIndexFile and the
//                    MultiCollection loop (id + Count in the first 8 bytes)
//   map blocks       MAP_BLOCK in xuocore/mulstruct.h (header + 64 cells)
//   multimap         src/MultiMap.cpp (i32 bounds, then an RLE body)
//   sound            src/Managers/SoundManager.cpp CreateWaveFile wraps the
//                    payload in its own WAV header: the payload is samples

#include "vfs_stub.h"

#include <common/str.h>

#include <string.h>

namespace
{

// little-endian helpers (the payloads follow the on-disk formats)
void put_u16(uint8_t *dst, uint16_t v)
{
    dst[0] = uint8_t(v);
    dst[1] = uint8_t(v >> 8);
}

void put_u32(uint8_t *dst, uint32_t v)
{
    put_u16(dst, uint16_t(v));
    put_u16(dst + 2, uint16_t(v >> 16));
}

// header length written for `kind` on an `ul`-byte payload (0: pure zeros).
// Headers are all-or-nothing: a truncated header would let the game's real
// parsers walk bytes beyond the stub (inside the mapped file), so payloads
// smaller than the kind's header degrade to plain zeros - which every one of
// those readers rejects safely (bad width / bad bounds warnings)
size_t stub_header_len(vfs_stub_kind kind, uint64_t ul, bool is_uop)
{
    switch (kind)
    {
        case vfs_stub_art:
            return ul >= 14 ? size_t(14) : size_t(0); // flags, w, h, lineOffset, empty run
        case vfs_stub_gump:
            // UOP: w(4) + h(4) + lookup[0](4); MUL: lookup[0](4) only -
            // lookup[0] == the row count, so every row parses zero run blocks
            return is_uop ? (ul >= 12 ? size_t(12) : size_t(0))
                          : (ul >= 4 ? size_t(4) : size_t(0));
        case vfs_stub_multi:    // id + Count=0 (MultiCollection loop)
        case vfs_stub_multimap: // i32 Width=1, i32 Height=1 (src/MultiMap.cpp)
            return ul >= 8 ? size_t(8) : size_t(0);
        case vfs_stub_anim:
            // full 40-byte header only when the fixed-offset reads of
            // UopReadAnimationFrameInfo stay inside the block: frame (16)
            // + palette (512) + AnimationFrameInfo (8) must fit behind the
            // header Offset (40, or 0 for squeezed layouts >= 536)
            return ul >= 536 ? size_t(40) : size_t(0);
        default:
            return size_t(0);
    }
}

// builds the kind's header into dst (exactly stub_header_len bytes; dst must
// hold at least 40 bytes - the largest header)
void stub_build_header(vfs_stub_kind kind, uint64_t ul, bool is_uop, uint8_t *dst)
{
    memset(dst, 0, 40);
    switch (kind)
    {
        case vfs_stub_art:
        {
            // GetArtPixels run branch: [0..4) flags, [4..6) width, [6..8)
            // height, then lineOffsets[height] and the {XOffs,Run} rows. A
            // 1x1 tile whose first (and only) row starts with an empty run
            // terminates the row loop immediately; all remaining bytes stay
            // zero (transparent). The first 8 bytes double as near-zero
            // pixel data when the same pack serves raw land tiles: two 1s
            // among 1012 u16 reads.
            put_u16(dst + 4, 1); // width
            put_u16(dst + 6, 1); // height
            break;
        }
        case vfs_stub_gump:
        {
            // UOP blocks: [0..4) Width, [4..8) Height (UopReadIndexFile reads
            // them into CIndexObject, then skips 8 bytes), then lookup[0] =
            // the exact row count (DataSize/4). The dims MUST stay 0x0: the
            // index build bakes them, so an invented 1x1 would make a healed
            // re-decode write W*H runs into a 1x1 pixel vector (no bounds
            // check in GetGumpPixels) - 0x0 short-circuits the reader
            // (blocksize == 0 warns and returns empty) and the hot-swap
            // refresh re-reads the real dims from the healed payload
            // (vfs_refresh_index). The row loop would parse ZERO run blocks
            // for any payload size and no byte beyond the header is ever
            // dereferenced. MUL payloads start at the lookup (the idx carries
            // the real width/height); the same trick yields zero work per row.
            if (is_uop)
            {
                put_u32(dst, 0);                              // width
                put_u32(dst + 4, 0);                          // height
                put_u32(dst + 8, uint32_t((ul - 8) / 4));     // lookup[0]
            }
            else
            {
                put_u32(dst, uint32_t(ul / 4)); // lookup[0]
            }
            break;
        }
        case vfs_stub_multi:
            put_u32(dst, 0);     // graphic id
            put_u32(dst + 4, 0); // Count = 0: empty multi
            break;
        case vfs_stub_multimap:
            put_u32(dst, 1);     // Width = 1
            put_u32(dst + 4, 1); // Height = 1; the zero RLE body draws nothing
            break;
        case vfs_stub_anim:
        {
            // UopAnimationHeader (40 bytes) with FrameCount = 0: no frames,
            // an invisible creature (AnimationDirFrames{frames,0}).
            put_u32(dst + 8, uint32_t(ul));        // DecompressedSize
            put_u32(dst + 28, 40);                 // HeaderSize
            put_u32(dst + 36, ul >= 576 ? 40 : 0); // Offset (see header len)
            break;
        }
        default:
            break;
    }
}

// the repeating map-block pattern: one MAP_BLOCK (mulstruct.h: header + 64
// cells of {u16 tile, i8 z}) of flat walkable land
const size_t MAP_BLOCK_SIZE = 196;

uint8_t stub_map_block_byte(size_t offset_in_block)
{
    if (offset_in_block < 4)
        return 0; // MAP_BLOCK::Header
    const size_t cell = (offset_in_block - 4) % 3;
    return cell == 0 ? uint8_t(VFS_STUB_MAP_FLAT_TILE_ID) : uint8_t(0);
}

} // namespace

vfs_stub_kind vfs_stub_kind_for(const char *file_name)
{
    if (file_name == nullptr)
        return vfs_stub_misc;
    char lower[128] = {};
    const size_t len = strlen(file_name);
    if (len >= sizeof(lower))
        return vfs_stub_misc;
    memcpy(lower, file_name, len);
    str_lower(lower);
    const auto has = [lower](const char *needle) { return strstr(lower, needle) != nullptr; };
    const auto starts = [lower](const char *prefix) {
        return strncmp(lower, prefix, strlen(prefix)) == 0;
    };
    // ordered longest-first: "multimap" before "multi", "gump" before "map",
    // "animation*" before "anim"
    if (has("multimap"))
        return vfs_stub_multimap;
    if (has("gump"))
        return vfs_stub_gump;
    if (has("animationframe") || has("animationsequence") || starts("anim"))
        return vfs_stub_anim;
    if (starts("music"))
        return vfs_stub_music;
    if (starts("sound"))
        return vfs_stub_sound;
    if (has("texmaps"))
        return vfs_stub_texture;
    if (starts("light"))
        return vfs_stub_light;
    if (starts("multi"))
        return vfs_stub_multi;
    if (starts("map") || has("mapdif"))
        return vfs_stub_map_block;
    if (starts("art") || has("artdif"))
        return vfs_stub_art;
    if (starts("speech"))
        return vfs_stub_speech;
    if (starts("cliloc") || has("string_dictionary"))
        return vfs_stub_cliloc;
    return vfs_stub_misc; // statics/staidx/unknown: zero-length semantics
}

size_t vfs_stub_min_payload(vfs_stub_kind kind)
{
    switch (kind)
    {
        case vfs_stub_art:
            return 14; // width/height/lineOffset/empty run must be ours
        case vfs_stub_gump:
            return 12; // width/height/lookup (UOP; MUL needs only 4)
        case vfs_stub_anim:
            return 536; // header + frame + palette + frame info (see above)
        case vfs_stub_multi:
        case vfs_stub_multimap:
            return 8;
        default:
            return 0; // zeros kinds parse at any size
    }
}

size_t vfs_stub_fill_slice(
    vfs_stub_kind kind,
    uint64_t ul,
    bool is_uop,
    uint64_t skip,
    uint8_t *dst,
    size_t cap)
{
    if (dst == nullptr || cap == 0 || ul == 0 || skip >= ul)
        return 0;
    if (uint64_t(cap) > ul - skip)
        cap = size_t(ul - skip);

    size_t nonzero = 0;
    if (kind == vfs_stub_map_block)
    {
        // the MAP_BLOCK pattern repeats payload-aligned: block boundaries sit
        // at multiples of 196 bytes in every real map file
        for (size_t i = 0; i < cap; ++i)
        {
            const uint8_t byte = stub_map_block_byte(size_t((skip + i) % MAP_BLOCK_SIZE));
            dst[i] = byte;
            nonzero += byte != 0 ? 1u : 0u;
        }
        return nonzero;
    }

    // remaining kinds: a little header followed by zeros
    memset(dst, 0, cap);
    const size_t prefix = stub_header_len(kind, ul, is_uop);
    if (skip < prefix)
    {
        uint8_t scratch[40];
        stub_build_header(kind, ul, is_uop, scratch);
        const size_t n = prefix - skip < cap ? size_t(prefix - skip) : cap;
        memcpy(dst, scratch + skip, n);
        for (size_t i = 0; i < n; ++i)
            nonzero += dst[i] != 0 ? 1u : 0u;
    }
    return nonzero;
}

size_t vfs_stub_fill(vfs_stub_kind kind, uint64_t ul, bool is_uop, uint8_t *dst, size_t cap)
{
    return vfs_stub_fill_slice(kind, ul, is_uop, 0, dst, cap);
}

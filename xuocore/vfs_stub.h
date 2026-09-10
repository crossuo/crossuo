// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// vfs_stub: placeholder asset providers.
//
// When a byte range cannot be fetched (offline, 404, timeout), the trap serves
// a synthetic payload instead of raw zeros: a minimal but syntactically valid
// asset for its kind, sized exactly to the plan's `ul`, so the game's real
// parsers decode it without changes - invisible item, blank gump, flat ground,
// silence. Payloads are deterministic pure functions of (kind, ul, offset):
// every payload is a fixed little header (kind-specific) followed by zeros,
// except the map block, which repeats one MAP_BLOCK pattern.
//
// Stubs are written through the existing writer as Flags=0 (uncompressed)
// blocks, so every consumer takes the memcpy path of DecompressBlock.
// Zero-content payloads (land tiles, texture/light pixels, sound samples,
// string tables) degrade to plain zero-fill: the sparse file already
// reads as zeros there.

#pragma once

#include <stdint.h>

#include <stddef.h>

// asset kinds; the trap derives the kind from the producing plan file name
// (the pack/loose file listed in the plan), never from asset content
enum vfs_stub_kind : uint8_t
{
    vfs_stub_art = 0,    // art.mul / artLegacyMUL.uop: 1x1 transparent run
    vfs_stub_gump,       // gumpart*.mul/.uop: transparent (UOP: 0x0 w/h header)
    vfs_stub_land,       // zero-color land tile
    vfs_stub_texture,    // texmaps.mul: zeros (plain ground)
    vfs_stub_light,      // light.mul: zeros (no light overlay)
    vfs_stub_sound,      // sound*.mul/.uop: zero samples (silent wave)
    vfs_stub_music,      // Music/**: zeros (silence until it arrives)
    vfs_stub_anim,       // AnimationFrame*.uop: empty UOP animation header
    vfs_stub_map_block,  // map*/mapdif*: 64x64 flat tile block
    vfs_stub_multi,      // multi.mul / MultiCollection.uop: empty multi
    vfs_stub_cliloc,     // string tables: zeros (T0, never stubbed in practice)
    vfs_stub_speech,     // speech.mul: zeros (T0, never stubbed in practice)
    vfs_stub_multimap,   // Multimap.rle: 1x1 map
    vfs_stub_misc,       // everything else (statics ranges included): zeros
    vfs_stub_kind_count
};

// flat land tile id written by the map-block generator. Tile 2 keeps a
// streamed-in map region walkable ("flat ground") instead of void;
// the value is a constant on purpose: the tiledata tables may not be loaded
// (or may themselves be streaming) when a map block is stubbed.
#define VFS_STUB_MAP_FLAT_TILE_ID 2

// kind for a plan file name ("artLegacyMUL.uop", "map0.mul", ...): matches
// the stub-kind-to-file mapping case-insensitively; unknown files map to
// vfs_stub_misc
vfs_stub_kind vfs_stub_kind_for(const char *file_name);

// smallest payload that keeps the kind's parser reading inside its own block
// (0: no in-payload minimum). Shorter payloads still parse - the sprite
// readers tolerate trailing data inside the mapped file - but the animation
// reader walks a fixed-offset layout in a heap scratch buffer, so short anim
// stubs degrade to plain zeros instead.
size_t vfs_stub_min_payload(vfs_stub_kind kind);

// slice generator for page-granular placement: fills dst with payload bytes
// [skip, skip + cap) of the `ul`-byte payload of a pack (is_uop) or loose
// asset. Returns the number of NONZERO bytes written (0 => the slice is all
// zeros: the caller may serve plain zeros). Never reads or writes out of
// bounds; a slice past the payload end (skip >= ul) writes nothing.
size_t vfs_stub_fill_slice(
    vfs_stub_kind kind,
    uint64_t ul,
    bool is_uop,
    uint64_t skip,
    uint8_t *dst,
    size_t cap);

// whole-payload convenience: fills dst with the payload's first `cap` bytes
// (callers pass cap >= ul for the full payload). Returns the number of
// nonzero bytes written.
size_t vfs_stub_fill(vfs_stub_kind kind, uint64_t ul, bool is_uop, uint8_t *dst, size_t cap);

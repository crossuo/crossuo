// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 CrossUO Team

// art replacer: gump/art/land graphic replacements from loose image files
// on disk. Lookups are lazy and id-only: the replacement is resolved under
// <UO path>/ExternalImages/{gumps,art,land}/<id>.<ext> on first use and
// cached. Nothing is scanned up front on purpose - the patch install writes
// those files while the game is already running (the login screen draws long
// before the streaming install converges), and a directory scan taken at
// init would never see them. Patch data can provide the same tree (the aops
// planner installs ExternalImages/ entries under their relative path), so a
// shard ships replacements without mul/uop patching.

#pragma once

#include <stdint.h>
#include <vector>

#include <common/fs.h>

enum ArtReplacerKind
{
    ART_REPLACER_GUMP = 0,
    ART_REPLACER_ART = 1,
    ART_REPLACER_LAND = 2,
};

// records the images root (<install_dir>/ExternalImages, case-insensitive);
// idempotent, no directory scan
void art_replacer_init(const fs_path &install_dir);

// replacement pixels in the engine's 16-bit 5551 color-key format (0 =
// transparent, opaque = 0x8000 | RGB555), top-down rows. false when no
// replacement exists for the id (a miss is probed, not cached: the patch
// install can land the file at any moment) or the image cannot be decoded.
// The same decoder serves every image the client loads from disk (login
// theme [image] files included): one layout, one code path.
bool art_replacer_decode(const fs_path &path, std::vector<uint16_t> &pixels, short &width, short &height);
// same decoder for bytes already in memory (login theme images fetched by
// url, including the web build where no cache file is written)
bool art_replacer_decode_memory(
    const uint8_t *data, size_t size, std::vector<uint16_t> &pixels, short &width, short &height);
bool art_replacer_pixels(
    int kind, uint16_t id, std::vector<uint16_t> &pixels, short &width, short &height);

// cheap "is there a replacement for this id" (resolved and cached)
bool art_replacer_has(int kind, uint16_t id);

// re-probes the images root when it was missing at init time: a streamed
// install fetches the ExternalImages tree after the replacer is initialized,
// and a running patch install can land it later (cheap no-op once resolved,
// called on every lookup miss path)
void art_replacer_update_root();

void art_replacer_init(const fs_path &install_dir);

// replacement pixels in the engine's 16-bit 5551 color-key format (0 =
// transparent, opaque = 0x8000 | RGB555), top-down rows. false when no
// replacement exists for the id (a miss is probed, not cached: the patch
// install can land the file at any moment) or the image cannot be decoded.
// The same decoder serves every image the client loads from disk (login
// theme [image] files included): one layout, one code path.
bool art_replacer_decode(const fs_path &path, std::vector<uint16_t> &pixels, short &width, short &height);
bool art_replacer_pixels(
    int kind, uint16_t id, std::vector<uint16_t> &pixels, short &width, short &height);

// cheap "is there a replacement for this id" (resolved and cached)
bool art_replacer_has(int kind, uint16_t id);

// Shared image decode: png/bmp/jpg -> 32bpp words for CSprite::LoadSprite32
// (one word per pixel, R in the low byte, A in the high one, rows bottom-up:
// the layout the renderer uploads as GL_RGBA8). stb delivers top-down RGBA,
// so the rows are flipped and the channels swizzled here. Used by the art
// replacer and the login theme images.
bool art_replacer_decode_image32(
    const fs_path &path, std::vector<uint32_t> &pixels, int &width, int &height);

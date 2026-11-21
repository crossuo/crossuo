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
// install can land the file at any moment) or the image cannot be decoded
bool art_replacer_pixels(
    int kind, uint16_t id, std::vector<uint16_t> &pixels, short &width, short &height);

// cheap "is there a replacement for this id" (resolved and cached)
bool art_replacer_has(int kind, uint16_t id);

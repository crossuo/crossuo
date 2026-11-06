// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <vector>

struct CIndexObject;

// Shared UO pixel decoders: the data-file parsing the client's UOFileReader
// and the standalone asset viewer both need. They only touch the xuocore data
// tables (g_Index/g_Data/g_FileManager) - no client globals - so any target
// linking xuocore decodes the same bytes the same way.

// Optional client-side art substitutions. The client replaces some stored art
// with pre-rendered tiles (trees rendered as stumps, cave floors darkened) and
// needs those results inside the same RLE walk; the standalone viewer always
// shows the stored art. Implementations are supplied by the host and the
// parser never dereferences a null one.
struct UoArtFilters
{
    // `stump` out-parameter: true when the tile is drawn from `pixels` (a
    // width x height 16-bit buffer owned by the host) instead of the file
    bool (*tree_tile)(uint16_t id, bool &stump, short &width, short &height,
                      const uint16_t *&pixels);
    // darken the transparent border of cave-floor tiles
    bool (*cave_tile)(uint16_t id);
};

// art.mul / artLegacyMUL.uop block to 16-bit pixels (0 = transparent, 0x8000
// alpha bit set on visible pixels). `run` selects static art (RLE rows) over
// land art (44x44 strips). io.Width/io.Height are written for the caller that
// only wants the size. `filters` may be null.
std::vector<uint16_t> uo_art_pixels(uint16_t id, CIndexObject &io, bool run, short &width,
                                    short &height, const UoArtFilters *filters);

// Gump block (gumpart.mul / gumpartLegacyMUL.uop) to 16-bit UO color pixels
// (0 = transparent, 0x8000 alpha bit set on visible pixels). The layout is
// learned into io.Width/io.Height exactly like the client reader does.
std::vector<uint16_t> uo_gump_pixels(CIndexObject &io);

// light.mul block to 16-bit pixels (same expansion as the client reader).
bool uo_light_pixels(CIndexObject &io, std::vector<uint16_t> &pixels, int &w, int &h);

// texmaps.mul block to 16-bit pixels (64x64 or 128x128 by DataSize).
bool uo_texmap_pixels(CIndexObject &io, std::vector<uint16_t> &pixels, int &w, int &h);

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

// 16-bit RGB555 to the client color-manager layout (0x00BBGGRR packed; the
// same table conversion the color manager and the texture cache use).
uint32_t uo_color16_to_32(uint16_t c);

// Hue lookup (hues.mul): the entry's color table is indexed by the top five
// bits of `c` (the value's red channel in RGB555). The hue is 1-based into
// the group/entry table (see uo_hue_index); 0 or out of range returns c.
uint16_t uo_hue_get_color16(uint16_t c, uint16_t color);
// (group, entry) of a 1-based hue id; false when out of range.
bool uo_hue_index(uint16_t color, int &group, int &entry);
// the partial-hue 32-bit color of a pixel (client color manager layout:
// 0xAABBGGRR), 0 when the hue is invalid.
uint32_t uo_hue_partial_color16(uint16_t c, uint16_t color);

// radarcol.mul entry for a graphic (0 when out of range).
uint16_t uo_radar_color(int id);

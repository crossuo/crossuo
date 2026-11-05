// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// The viewer core's window into the shared UO data readers (xuocore). Only the
// subset the viewer needs is exposed here so the core does not have to pull in
// xuocore/uodata_reader.h (and with it the file-manager layering) directly.
// Both hosts - the client and the standalone app - link the same definitions.

#if defined(XUO_ASSISTANT)

#include <stdint.h>
#include <vector>

struct CIndexObject;

// art.mul / artLegacyMUL.uop pixels, exactly as stored (run = true static
// art, false land art). Both hosts forward to the shared xuocore parser
// (uo_art_pixels): the client passes its stump/cave tile filters, the
// standalone app passes none and sees the stored art.
std::vector<uint16_t>
AssetViewer_DecodeArt(uint16_t id, CIndexObject &io, bool run, short &width, short &height);
// gumpart pixel walk (uop + mul layouts, shared dimensions cache on the entry)
std::vector<uint16_t> uo_gump_pixels(CIndexObject &io);
// light.mul and texmaps.mul blocks
bool uo_light_pixels(CIndexObject &io, std::vector<uint16_t> &pixels, int &w, int &h);
bool uo_texmap_pixels(CIndexObject &io, std::vector<uint16_t> &pixels, int &w, int &h);

// hue/color helpers (hues.mul and the RGB555 expansion table)
uint32_t uo_color16_to_32(uint16_t c);
uint16_t uo_hue_get_color16(uint16_t c, uint16_t color);
uint32_t uo_hue_partial_color16(uint16_t c, uint16_t color);
uint16_t uo_radar_color(int id);

#endif // XUO_ASSISTANT

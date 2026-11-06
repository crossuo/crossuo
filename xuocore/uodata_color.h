// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <cstdint>

// 16-bit RGB555 to the client color-manager layout (0x00BBGGRR packed; the
// same table conversion the color manager and the texture cache use).
uint32_t uo_color16_to_32(uint16_t c);
uint16_t uo_color32_to_16(int c);
uint16_t uo_color_convert_to_gray(uint16_t c);

// Hue lookup (hues.mul): the entry's color table is indexed by the top five
// bits of `c` (the value's red channel in RGB555). The hue is 1-based into
// the group/entry table (see uo_hue_index); 0 or out of range returns c.
uint16_t uo_hue_get_color16(uint16_t c, uint16_t color);
// return black if no hue found
uint32_t uo_hue_get_color_poly(uint16_t c, uint16_t color);
uint32_t uo_hue_get_color_font(uint16_t c, uint16_t color);
// (group, entry) of a 1-based hue id; false when out of range.
bool uo_hue_index(uint16_t color, int &group, int &entry);
// the partial-hue 32-bit color of a pixel (client color manager layout:
// 0xAABBGGRR), 0 when the hue is invalid.
uint32_t uo_hue_partial_color16(uint16_t c, uint16_t color);

// radarcol.mul entry for a graphic (0 when out of range).
uint16_t uo_radar_color(int id);

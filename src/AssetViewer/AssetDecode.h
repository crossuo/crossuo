// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#if defined(XUO_ASSISTANT)

#include <stdint.h>

#include <vector>

// Read-only pixel decoders for the effects assets that have no getter in
// UOFileReader: light.mul (additive lightmaps, one gray byte per pixel) and
// texmaps.mul (raw 16-bit color rows, 0 = transparent). Both mirror the
// parsing of UOFileReader::ReadLight / ReadTexture byte-for-byte but return
// plain 16-bit buffers - no client texture (CSprite) is created.

// Decodes g_Index.m_Light[id] into a Width*Height buffer; each source byte b
// expands to (b<<10)|(b<<5)|b, keeping 0 fully transparent. Returns false
// when the entry is missing or the index carries no size.
bool AssetViewer_DecodeLight(uint16_t id, std::vector<uint16_t> &pixels, int &w, int &h);

// Decodes g_Index.m_Texture[id] (DataSize 0x2000 = 64x64, 0x8000 = 128x128)
// into a raw 16-bit buffer; 0 stays transparent. Returns false for missing
// entries or unexpected data sizes.
bool AssetViewer_DecodeTexture(uint16_t id, std::vector<uint16_t> &pixels, int &w, int &h);

#endif // XUO_ASSISTANT

// SPDX-License-Identifier: AGPL-3.0-or-later

#include "AssetDecode.h"
#include <cstdint>

#if defined(XUO_ASSISTANT)

#include <string.h>


#include <xuocore/uodata.h> // g_Index, g_FileManager

// Mirrors UOFileReader::ReadLight's parsing: lightidx gives Address plus the
// Width/Height bytes; the block holds Width*Height gray bytes, each expanded
// to (b<<10)|(b<<5)|b with the 0x8000 alpha bit for nonzero values. The web
// lane resolves the payload through the windowed reader exactly like
// ReadLight does (io.Address is the file offset when file-backed).
bool AssetViewer_DecodeLight(uint16_t id, std::vector<uint16_t> &pixels, int &w, int &h)
{
    if ((uint32_t)id >= (uint32_t)MAX_LIGHTS_DATA_INDEX_COUNT)
    {
        return false;
    }
    const CIndexLight &io = g_Index.m_Light[id];
    if (io.Address == 0 && io.UopBlock == nullptr)
    {
        return false;
    }
    w = io.Width;
    h = io.Height;
    if (w <= 0 || h <= 0)
    {
        return false;
    }
#if defined(XUO_EMSC)
    std::vector<uint8_t> lightBlock;
    const uint8_t *p = nullptr;
    if (g_FileManager.m_LightMul.FileBacked && g_FileManager.m_LightMul.Loaded())
    {
        lightBlock.resize(size_t(w) * h);
        g_FileManager.m_LightMul.Seek(io.Address);
        g_FileManager.m_LightMul.ReadDataLE(lightBlock.data(), uint32_t(lightBlock.size()));
        p = lightBlock.data();
    }
    else
    {
        p = reinterpret_cast<const uint8_t *>(io.Address);
    }
#else
    const uint8_t *p = (const uint8_t *)io.Address;
#endif
    if (p == nullptr)
    {
        return false;
    }
    pixels.resize(size_t(w) * h);
    for (int i = 0; i < h; i++)
    {
        const int pos = i * w;
        for (int j = 0; j < w; j++)
        {
            const uint16_t val = uint16_t((*p << 10) | (*p << 5) | *p);
            pixels[pos + j] = (val != 0u ? 0x8000 : 0) | val;
            p++;
        }
    }
    return true;
}

// Mirrors UOFileReader::ReadTexture's parsing: texidx gives Address and the
// raw DataSize (0x2000 = 64x64, 0x8000 = 128x128); the block is a plain
// Width*Height array of 16-bit colors where 0 means transparent. Unlike
// ReadTexture the index color (io.Color) is intentionally not applied here -
// hue previewing is the viewer's job (texture cache hue argument).
bool AssetViewer_DecodeTexture(uint16_t id, std::vector<uint16_t> &pixels, int &w, int &h)
{
    if ((uint32_t)id >= (uint32_t)MAX_LAND_TEXTURES_DATA_INDEX_COUNT)
    {
        return false;
    }
    const CIndexObject &io = g_Index.m_Texture[id];
    if (io.Address == 0 && io.UopBlock == nullptr)
    {
        return false;
    }
    if (io.DataSize == 0x2000)
    {
        w = 64;
        h = 64;
    }
    else if (io.DataSize == 0x8000)
    {
        w = 128;
        h = 128;
    }
    else
    {
        return false; // unexpected size: treated like missing data
    }
#if defined(XUO_EMSC)
    std::shared_ptr<std::vector<uint8_t>> texBlock;
    const uint16_t *p = nullptr;
    if (vfs_stream_texmaps_windowed())
    {
        // stream-file lane: texmaps.mul is streamable metadata and not
        // materialized - the index entry holds the relative offset; resolve
        // the bytes as a registry window (null while cold: skip like missing
        // data)
        const uint8_t *window = vfs_stream_read_texmaps(size_t(io.Address), io.DataSize);
        if (window == nullptr)
        {
            return false;
        }
        p = (const uint16_t *)window;
    }
    else if (g_FileManager.m_TextureMul.FileBacked && g_FileManager.m_TextureMul.Loaded())
    {
        // file-backed texmaps.mul: read the texture through the windowed
        // reader (io.Address is the payload's file offset)
        texBlock = std::make_shared<std::vector<uint8_t>>(io.DataSize, 0);
        g_FileManager.m_TextureMul.Seek(io.Address);
        g_FileManager.m_TextureMul.ReadDataLE(texBlock->data(), io.DataSize);
        p = (const uint16_t *)texBlock->data();
    }
#else
    const uint16_t *p = (const uint16_t *)io.Address;
#endif
    if (p == nullptr)
    {
        return false;
    }
    pixels.resize(size_t(w) * h);
    memcpy(pixels.data(), p, size_t(w) * h * sizeof(uint16_t));
    return true;
}

#endif // XUO_ASSISTANT

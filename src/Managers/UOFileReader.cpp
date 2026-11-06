// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include <algorithm>
#include <cstdint>
#include "UOFileReader.h"
#include <vector>
#include <xuocore/vfs.h>
#include "ColorManager.h"
#include <common/logging/logging.h>
#include <xuocore/uodata.h>
#include <xuocore/uodata_reader.h> // uo_art_pixels, uo_gump_pixels
#include <xuocore/uodata_color.h>
#include "../CrossUO.h"
#include "../StumpsData.h"
#include "../Sprite.h"
#include "xuocore/mulstruct.h"

UOFileReader g_UOFileReader;

std::vector<uint16_t> UOFileReader::GetGumpPixels(CIndexObject &io)
{
    // the shared xuocore walk (the standalone asset viewer calls this same
    // function): uop and mul layouts, dimensions cached on the index entry
    return uo_gump_pixels(io);
}

CSprite *UOFileReader::ReadGump(CIndexObject &io)
{
    std::vector<uint16_t> pixels = GetGumpPixels(io);
    if (pixels.empty())
    {
        return nullptr;
    }
    auto *spr = new CSprite();
    spr->LoadSprite16(io.Width, io.Height, pixels.data());
    return spr;
}

namespace
{
// the game's own art substitutions, handed to the shared parser: trees are
// drawn from the pre-rendered stump tiles and cave floors are darkened. Both
// read client globals that xuocore does not own, so they live here.
bool ArtTreeTile(uint16_t id, bool &stump, short &width, short &height, const uint16_t *&pixels)
{
    int stumpIndex = 0;
    if (!g_Game.IsTreeTile(id, stumpIndex))
    {
        return false;
    }
    stump = true;
    if (stumpIndex == g_StumpHatchedID)
    {
        width = g_StumpHatchedWidth;
        height = g_StumpHatchedHeight;
        pixels = (const uint16_t *)g_StumpHatched;
    }
    else
    {
        width = g_StumpWidth;
        height = g_StumpHeight;
        pixels = (const uint16_t *)g_Stump;
    }
    return true;
}

bool ArtCaveTile(uint16_t id)
{
    return g_Game.IsCaveTile(id);
}

const UoArtFilters s_ArtFilters = {ArtTreeTile, ArtCaveTile};
} // namespace

std::vector<uint16_t> UOFileReader::GetArtPixels(
    uint16_t id,
    CIndexObject &io,
    bool run,
    short &width,
    short &height,
    bool applyTileFilters)
{
    // the shared parser (xuocore) does the decoding; the game-only tile
    // substitutions are injected, and the standalone asset viewer calls the
    // same function without them
    return uo_art_pixels(id, io, run, width, height, applyTileFilters ? &s_ArtFilters : nullptr);
}

CSprite *UOFileReader::ReadArt(uint16_t id, CIndexObject &io, bool run)
{
    int16_t width = 0;
    int16_t height = 0;
    std::vector<uint16_t> pixels = GetArtPixels(id, io, run, width, height);
    if (pixels.empty())
    {
        return nullptr;
    }
    int minX = width;
    int minY = height;
    int maxX = 0;
    int maxY = 0;
    if (!run)
    {
        maxX = 44;
        maxY = 44;
        bool allBlack = true;
        {
            int pos = 0;
            for (int i = 0; i < 44; i++)
            {
                for (int j = 0; j < 44; j++)
                {
                    if (pixels[pos++] != 0u)
                    {
                        i = 44;
                        allBlack = false;
                        break;
                    }
                }
            }
        }

        ((CIndexObjectLand *)&io)->AllBlack = allBlack;
        if (allBlack)
        {
            for (int i = 0; i < 22; i++)
            {
                const int start = (22 - ((int)i + 1));
                int pos = (int)i * 44 + start;
                const int end = start + ((int)i + 1) * 2;
                for (int j = start; j < end; j++)
                {
                    pixels[pos++] = 0x8000;
                }
            }

            for (int i = 0; i < 22; i++)
            {
                int pos = ((int)i + 22) * 44 + (int)i;
                const int end = (int)i + (22 - (int)i) * 2;
                for (int j = i; j < end; j++)
                {
                    pixels[pos++] = 0x8000;
                }
            }
        }
    }
    else
    {
        int pos = 0;
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                if (pixels[pos++] != 0u)
                {
                    minX = std::min(minX, int(x));
                    maxX = std::max(maxX, int(x));
                    minY = std::min(minY, int(y));
                    maxY = std::max(maxY, int(y));
                }
            }
        }
    }

    auto *spr = new CSprite();
    spr->ImageOffsetX = minX;
    spr->ImageOffsetY = minY;
    spr->ImageWidth = maxX - minX;
    spr->ImageHeight = maxY - minY;
    spr->LoadSprite16(width, height, pixels.data());
    return spr;
}

CSprite *UOFileReader::ReadTexture(CIndexObject &io)
{
    uint16_t color = io.Color;
    uint16_t w = 64;
    uint16_t h = 64;
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
        Warning(Data, "UOFileReader::ReadTexture bad data size: %d", io.DataSize);
        return nullptr;
    }

    std::vector<uint16_t> pixels(w * h);
    uint16_t *P = (uint16_t *)io.Address;
#if defined(XUO_EMSC)
    std::shared_ptr<std::vector<uint8_t>> texBlock;
    if (vfs_stream_texmaps_windowed())
    {
        // stream-file lane: texmaps.mul is streamable metadata and not
        // materialized - the index entry holds the relative offset; resolve
        // the bytes as a registry window (null while cold: skip like missing
        // data)
        const uint8_t *window = vfs_stream_read_texmaps(size_t(io.Address), io.DataSize);
        if (window == nullptr)
        {
            return nullptr;
        }
        P = (uint16_t *)window;
    }
    else if (g_FileManager.m_TextureMul.FileBacked && g_FileManager.m_TextureMul.Loaded())
    {
        // file-backed texmaps.mul: read the texture through the windowed
        // reader (io.Address is the payload's file offset)
        texBlock = std::make_shared<std::vector<uint8_t>>(io.DataSize, 0);
        g_FileManager.m_TextureMul.Seek(io.Address);
        g_FileManager.m_TextureMul.ReadDataLE(texBlock->data(), io.DataSize);
        P = (uint16_t *)texBlock->data();
    }
    else
    {
        return nullptr; // no lane and no file: nothing to read (web)
    }
#endif
    for (int i = 0; i < h; i++)
    {
        int pos = (int)i * w;
        for (int j = 0; j < w; j++)
        {
            uint16_t val = *P++;
            if (color != 0u)
            {
                val = uo_hue_get_color16(val, color);
            }
            pixels[pos + j] = 0x8000 | val;
        }
    }

    const bool skipHitMask = true;
    auto *spr = new CSprite();
    spr->LoadSprite16(w, h, pixels.data(), skipHitMask);
    return spr;
}

CSprite *UOFileReader::ReadLight(CIndexObject &io)
{
    std::vector<uint16_t> pixels(io.Width * io.Height);
#if defined(XUO_EMSC)
    std::vector<uint8_t> lightBlock;
    const uint8_t *lightData = nullptr;
    if (g_FileManager.m_LightMul.FileBacked && g_FileManager.m_LightMul.Loaded())
    {
        // web: file-backed light.mul - read through the windowed reader
        // (io.Address is the payload's file offset)
        const uint32_t bytes = uint32_t(io.Width * io.Height);
        lightBlock.resize(bytes);
        g_FileManager.m_LightMul.Seek(io.Address);
        g_FileManager.m_LightMul.ReadDataLE(lightBlock.data(), bytes);
        lightData = lightBlock.data();
    }
    else
    {
        lightData = reinterpret_cast<const uint8_t *>(io.Address);
    }
    const uint8_t *p = lightData;
#else
    uint8_t *p = (uint8_t *)io.Address;
#endif
    for (int i = 0; i < io.Height; i++)
    {
        int pos = (int)i * io.Width;
        for (int j = 0; j < io.Width; j++)
        {
            uint16_t val = (*p << 10) | (*p << 5) | *p;
            p++;
            pixels[pos + j] = (val != 0u ? 0x8000 : 0) | val;
        }
    }

    const bool skipHitMask = true;
    auto *spr = new CSprite();
    spr->LoadSprite16(io.Width, io.Height, pixels.data(), skipHitMask);
    return spr;
}

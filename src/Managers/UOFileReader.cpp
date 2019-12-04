﻿// MIT License
// Copyright (C) August 2016 Hotride

#include "UOFileReader.h"
#include "ColorManager.h"
#include <xuocore/uodata.h>
#include "../CrossUO.h"
#include "../StumpsData.h"
#include "../Sprite.h"
#include "../Logging.h"

UOFileReader g_UOFileReader;

std::vector<uint16_t> UOFileReader::GetGumpPixels(CIndexObject &io)
{
    DEBUG_TRACE_FUNCTION;
    size_t dataStart = io.Address;
    uint32_t *lookupList = (uint32_t *)dataStart;

    int blocksize = io.Width * io.Height;

    std::vector<uint16_t> pixels;

    if (blocksize == 0)
    {
        Warning(Data, "UOFileReader::GetGumpPixels bad size:%i, %i", io.Width, io.Height);
        return pixels;
    }

    pixels.resize(blocksize);

    if (pixels.size() != blocksize)
    {
        Warning(
            Data, "Allocation pixels memory for GetGumpPixels failed (want size: %i)", blocksize);
        return pixels;
    }

    uint16_t color = io.Color;

    for (int y = 0; y < io.Height; y++)
    {
        int gSize = 0;

        if (y < io.Height - 1)
        {
            gSize = lookupList[y + 1] - lookupList[y];
        }
        else
        {
            gSize = (io.DataSize / 4) - lookupList[y];
        }

        GUMP_BLOCK *gmul = (GUMP_BLOCK *)(dataStart + lookupList[y] * 4);
        int pos = (int)y * io.Width;

        for (int i = 0; i < gSize; i++)
        {
            uint16_t val = gmul[i].Value;

            if ((color != 0u) && (val != 0u))
            {
                val = g_ColorManager.GetColor16(val, color);
            }

            uint16_t a = (val != 0u ? 0x8000 : 0) | val;

            int count = gmul[i].Run;

            for (int j = 0; j < count; j++)
            {
                pixels[pos++] = a;
            }
        }
    }

    return pixels;
}

CSprite *UOFileReader::ReadGump(CIndexObject &io)
{
    DEBUG_TRACE_FUNCTION;
    std::vector<uint16_t> pixels = GetGumpPixels(io);
    if (pixels.empty())
    {
        return nullptr;
    }
    auto spr = new CSprite();
    spr->LoadSprite16(io.Width, io.Height, pixels.data());
    return spr;
}

std::vector<uint16_t>
UOFileReader::GetArtPixels(uint16_t id, CIndexObject &io, bool run, short &width, short &height)
{
    DEBUG_TRACE_FUNCTION;
    uint16_t *P = (uint16_t *)io.Address;
    uint16_t color = io.Color;
    std::vector<uint16_t> pixels;
    if (!run) //raw tile
    {
        width = 44;
        height = 44;
        pixels.resize(44 * 44, 0);
        for (int i = 0; i < 22; i++)
        {
            int start = (22 - ((int)i + 1));
            int pos = (int)i * 44 + start;
            int end = start + ((int)i + 1) * 2;
            for (int j = start; j < end; j++)
            {
                uint16_t val = *P++;
                if ((color != 0u) && (val != 0u))
                {
                    val = g_ColorManager.GetColor16(val, color);
                }

                if (val != 0u)
                {
                    val = 0x8000 | val;
                }
                pixels[pos++] = val;
            }
        }

        for (int i = 0; i < 22; i++)
        {
            int pos = ((int)i + 22) * 44 + (int)i;
            int end = (int)i + (22 - (int)i) * 2;
            for (int j = i; j < end; j++)
            {
                uint16_t val = *P++;
                if ((color != 0u) && (val != 0u))
                {
                    val = g_ColorManager.GetColor16(val, color);
                }

                if (val != 0u)
                {
                    val = 0x8000 | val;
                }
                pixels[pos++] = val;
            }
        }
    }
    else //run tile
    {
        int stumpIndex = 0;
        if (g_Game.IsTreeTile(id, stumpIndex))
        {
            uint16_t *ptr = nullptr;
            if (stumpIndex == g_StumpHatchedID)
            {
                width = g_StumpHatchedWidth;
                height = g_StumpHatchedHeight;
                ptr = (uint16_t *)g_StumpHatched;
            }
            else
            {
                width = g_StumpWidth;
                height = g_StumpHeight;
                ptr = (uint16_t *)g_Stump;
            }

            int blocksize = width * height;
            pixels.resize(blocksize);
            if (pixels.size() != blocksize)
            {
                Warning(
                    Data,
                    "Allocation pixels memory for ReadArt::LandTile failed (want size: %i)",
                    blocksize);
                pixels.clear();
                return pixels;
            }

            for (int i = 0; i < blocksize; i++)
            {
                pixels[i] = ptr[i];
            }
        }
        else
        {
            uint16_t *ptr = (uint16_t *)(io.Address + 4);
            width = *ptr;
            if ((width == 0) || width >= 1024)
            {
                Warning(Data, "UOFileReader::ReadArt bad width:%i", width);
                return pixels;
            }

            ptr++;
            height = *ptr;
            if ((height == 0) || (height * 2) > 5120)
            {
                Warning(Data, "UOFileReader::ReadArt bad height:%i", height);
                return pixels;
            }

            ptr++;
            uint16_t *lineOffsets = ptr;
            uint8_t *dataStart = (uint8_t *)ptr + (height * 2);
            int X = 0;
            int Y = 0;
            uint16_t XOffs = 0;
            uint16_t Run = 0;
            int blocksize = width * height;
            pixels.resize(blocksize, 0);
            if (pixels.size() != blocksize)
            {
                Warning(
                    Data,
                    "Allocation pixels memory for ReadArt::StaticTile failed (want size: %i)",
                    blocksize);
                pixels.clear();
                return pixels;
            }

            ptr = (uint16_t *)(dataStart + (lineOffsets[0] * 2));
            while (Y < height)
            {
                XOffs = *ptr;
                ptr++;
                Run = *ptr;
                ptr++;
                if (XOffs + Run >= 2048)
                {
                    Warning(Data, "UOFileReader::ReadArt bad offset:%i, %i", XOffs, Run);
                    pixels.clear();
                    return pixels;
                }
                if (XOffs + Run != 0)
                {
                    X += XOffs;
                    int pos = Y * width + X;
                    for (int j = 0; j < Run; j++)
                    {
                        uint16_t val = *ptr++;
                        if (val != 0u)
                        {
                            if (color != 0u)
                            {
                                val = g_ColorManager.GetColor16(val, color);
                            }
                            val = 0x8000 | val;
                        }
                        pixels[pos++] = val;
                    }
                    X += Run;
                }
                else
                {
                    X = 0;
                    Y++;
                    ptr = (uint16_t *)(dataStart + (lineOffsets[Y] * 2));
                }
            }

            if ((id >= 0x2053 && id <= 0x2062) ||
                (id >= 0x206A && id <= 0x2079)) //Убираем рамку (если это курсор мышки)
            {
                for (int i = 0; i < width; i++)
                {
                    pixels[i] = 0;
                    pixels[(height - 1) * width + i] = 0;
                }

                for (int i = 0; i < height; i++)
                {
                    pixels[i * width] = 0;
                    pixels[i * width + width - 1] = 0;
                }
            }
            else if (g_Game.IsCaveTile(id))
            {
                for (int y = 0; y < height; y++)
                {
                    int startY = (y != 0 ? -1 : 0);
                    int endY = (y + 1 < height ? 2 : 1);
                    for (int x = 0; x < width; x++)
                    {
                        uint16_t &pixel = pixels[y * width + x];
                        if (pixel != 0u)
                        {
                            int startX = (x != 0 ? -1 : 0);
                            int endX = (x + 1 < width ? 2 : 1);
                            for (int i = startY; i < endY; i++)
                            {
                                int currentY = (int)y + (int)i;
                                for (int j = startX; j < endX; j++)
                                {
                                    int currentX = (int)x + (int)j;
                                    uint16_t &currentPixel = pixels[currentY * width + currentX];
                                    if (currentPixel == 0u)
                                    {
                                        pixel = 0x8000;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return pixels;
}

CSprite *UOFileReader::ReadArt(uint16_t id, CIndexObject &io, bool run)
{
    DEBUG_TRACE_FUNCTION;
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

    auto spr = new CSprite();
    spr->ImageOffsetX = minX;
    spr->ImageOffsetY = minY;
    spr->ImageWidth = maxX - minX;
    spr->ImageHeight = maxY - minY;
    spr->LoadSprite16(width, height, pixels.data());
    return spr;
}

CSprite *UOFileReader::ReadTexture(CIndexObject &io)
{
    DEBUG_TRACE_FUNCTION;
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
    for (int i = 0; i < h; i++)
    {
        int pos = (int)i * w;
        for (int j = 0; j < w; j++)
        {
            uint16_t val = *P++;
            if (color != 0u)
            {
                val = g_ColorManager.GetColor16(val, color);
            }
            pixels[pos + j] = 0x8000 | val;
        }
    }

    const bool skipHitMask = true;
    auto spr = new CSprite();
    spr->LoadSprite16(w, h, pixels.data(), skipHitMask);
    return spr;
}

CSprite *UOFileReader::ReadLight(CIndexObject &io)
{
    DEBUG_TRACE_FUNCTION;
    std::vector<uint16_t> pixels(io.Width * io.Height);
    uint8_t *p = (uint8_t *)io.Address;
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
    auto spr = new CSprite();
    spr->LoadSprite16(io.Width, io.Height, pixels.data(), skipHitMask);
    return spr;
}

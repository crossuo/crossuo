// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include <algorithm>
#include <memory>
#include "UOFileReader.h"
#include <xuocore/vfs.h>
#include "ColorManager.h"
#include <common/logging/logging.h>
#include <xuocore/uodata.h>
#include "../CrossUO.h"
#include "../StumpsData.h"
#include "../Sprite.h"

UOFileReader g_UOFileReader;

std::vector<uint16_t> UOFileReader::GetGumpPixels(CIndexObject &io)
{
    vfs_gump_dims_resolve(&io); // streamed hole-baked entries fetch + cache dims on first use
    std::vector<uint8_t> decoded;
    const uint32_t *lookupList = nullptr;
    int halfLen = 0;
    const uint8_t *dataStart = nullptr;

    const auto *uopBlock = io.UopBlock;
    if (uopBlock != nullptr && uopBlock->Flags != 0)
    {
        // compressed gump entries do not carry [width][height] in the mapped
        // payload: decode the block and parse the picture from the buffer,
        // caching the dimensions on this entry for the callers that only ask
        // for the size
        decoded = g_FileManager.m_GumpartLegacyMUL.GetData(uopBlock);
        if (decoded.size() < 8)
        {
            Warning(Data, "UOFileReader::GetGumpPixels could not decode the gump block");
            return {};
        }

        uint32_t width = 0;
        uint32_t height = 0;
        memcpy(&width, &decoded[0], sizeof(width));
        memcpy(&height, &decoded[4], sizeof(height));
        constexpr uint32_t maxGumpDimension = 2048;
        if (width == 0 || height == 0 || width > maxGumpDimension || height > maxGumpDimension)
        {
            Warning(
                Data,
                "UOFileReader::GetGumpPixels unsupported gump layout (w=%u h=%u)",
                width,
                height);
            return {};
        }

        io.Width = int32_t(width);
        io.Height = int32_t(height);
        lookupList = reinterpret_cast<const uint32_t *>(decoded.data() + 8);
        halfLen = int((decoded.size() - 8) >> 2);
    }
    else
    {
        lookupList = reinterpret_cast<const uint32_t *>(io.Address);
        halfLen = int(io.DataSize >> 2);
    }

#if defined(XUO_EMSC)
    // web: the pack is a table-only skeleton - stored blocks live in the
    // fetch layer, not at io.Address; the payload holder keeps the block
    // alive for this function's scope. Classic MUL gumps are file-backed and
    // read through a bounded window instead.
    std::shared_ptr<std::vector<uint8_t>> storedPayload;
    CDataReader mulReader;
    if (uopBlock != nullptr && uopBlock->Flags == 0)
    {
        storedPayload =
            vfs_asset_fetch_block_payload(uopBlock->Hash, uopBlock, false);
        if (storedPayload == nullptr || storedPayload->size() < 8)
        {
            Warning(Data, "UOFileReader::GetGumpPixels could not fetch the stored gump block");
            return {};
        }
        // skeleton-baked entries have no dims: the payload's first words
        // carry them, exactly like the compressed branch above
        if (io.Width == 0 || io.Height == 0)
        {
            uint32_t width = 0;
            uint32_t height = 0;
            memcpy(&width, storedPayload->data(), sizeof(width));
            memcpy(&height, storedPayload->data() + 4, sizeof(height));
            constexpr uint32_t maxGumpDimension = 2048;
            if (width == 0 || height == 0 || width > maxGumpDimension || height > maxGumpDimension)
            {
                Warning(
                    Data,
                    "UOFileReader::GetGumpPixels unsupported gump layout (w=%u h=%u)",
                    width,
                    height);
                return {};
            }
            io.Width = int32_t(width);
            io.Height = int32_t(height);
        }
        lookupList = reinterpret_cast<const uint32_t *>(storedPayload->data() + 8);
        halfLen = int((storedPayload->size() - 8) >> 2);
        dataStart = storedPayload->data() + 8;
    }
    else if (uopBlock != nullptr && uopBlock->Flags != 0)
    {
        // compressed UOP: the decode above already produced the block (the
        // native branch below does the same on desktop)
        dataStart = decoded.data() + 8; // past the extra width,height
    }
    else if (uopBlock == nullptr && g_FileManager.m_GumpMul.FileBacked &&
             g_FileManager.m_GumpMul.Loaded())
    {
        // classic MUL gump: io.Address is the payload's file offset and the
        // row walk below reads through the windowed reader. MUL gump blocks
        // carry NO [width][height] header (that is the UOP layout): the u32
        // row lookup table starts at block+0 with offsets relative to the
        // block start, and the dimensions come from the index entry
        // (CIndexGump). Same table arithmetic as the native tail below.
        auto &file = g_FileManager.m_GumpMul;
        if (!mulReader.Attach(file, io.Address, io.DataSize) || io.Width == 0 || io.Height == 0)
        {
            return {};
        }
        lookupList = nullptr; // u32 table resolved per row below (window reads)
        halfLen = int(io.DataSize >> 2);
        dataStart = nullptr;
    }
    else
    {
        return {}; // no fetchable block and no file-backed MUL gump
    }
#else
    // native: the mapping is the whole file - the picture starts past the
    // lookup table (compressed blocks were decoded above, classic gumps
    // point straight into the mapping). Statement-for-statement the same
    // tail HEAD ran after the EMSC region's #endif; the restructure only
    // moved it into the #else arm to give web its own lanes.
    dataStart = uopBlock != nullptr && uopBlock->Flags != 0
                    ? decoded.data() + 8
                    : reinterpret_cast<const uint8_t *>(io.Address);
#endif

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

#if defined(XUO_EMSC)
    // file-backed MUL gump: the same walk as the native loop below, with
    // the u32 lookup table and every row resolved through the windowed
    // reader. The table starts at block+0 (no [width][height] header in a
    // MUL gump block) and its offsets are relative to the block start;
    // dimensions come from the index entry.
    if (dataStart == nullptr && lookupList == nullptr)
    {
        std::vector<uint32_t> lookup(size_t(io.Height), 0);
        mulReader.ReadDataLE(
            (uint8_t *)lookup.data(), int(lookup.size()) * int(sizeof(uint32_t)));
        for (int y = 0; y < io.Height; y++)
        {
            int gSize = 0;
            if (y < io.Height - 1)
            {
                gSize = int(lookup[size_t(y) + 1] - lookup[size_t(y)]);
            }
            else
            {
                gSize = halfLen - int(lookup[size_t(y)]);
            }
            mulReader.Seek(size_t(lookup[size_t(y)]) * 4);
            int pos = (int)y * io.Width;
            for (int i = 0; i < gSize; i++)
            {
                GUMP_BLOCK gmul{};
                mulReader.ReadDataLE((uint8_t *)&gmul, sizeof(gmul));
                uint16_t val = gmul.Value;

                if ((color != 0u) && (val != 0u))
                {
                    val = g_ColorManager.GetColor16(val, color);
                }

                const uint16_t a = (val != 0u ? 0x8000 : 0) | val;

                int count = gmul.Run;

                // malformed blocks (or a half-written streamed one) must not
                // run past the pixel buffer - clamp instead of overflowing
                if (pos + count > blocksize)
                {
                    count = blocksize - pos;
                }

                for (int j = 0; j < count; j++)
                {
                    pixels[pos++] = a;
                }
            }
        }
        return pixels;
    }
#endif

    for (int y = 0; y < io.Height; y++)
    {
        int gSize = 0;

        if (y < io.Height - 1)
        {
            gSize = lookupList[y + 1] - lookupList[y];
        }
        else
        {
            gSize = halfLen - lookupList[y];
        }

        const GUMP_BLOCK *gmul = (const GUMP_BLOCK *)(dataStart + lookupList[y] * 4);
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

            // malformed blocks (or a half-written streamed one) must not
            // run past the pixel buffer - clamp instead of overflowing
            if (pos + count > blocksize)
            {
                count = blocksize - pos;
            }

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
    std::vector<uint16_t> pixels = GetGumpPixels(io);
    if (pixels.empty())
    {
        return nullptr;
    }
    auto *spr = new CSprite();
    spr->LoadSprite16(io.Width, io.Height, pixels.data());
    return spr;
}

std::vector<uint16_t>
UOFileReader::GetArtPixels(uint16_t id, CIndexObject &io, bool run, short &width, short &height)
{
#if defined(XUO_EMSC)
    // web: the art pack is a table-only skeleton - the block comes from the
    // punctual fetch layer; the holder keeps it alive for this function.
    // Classic MUL art is file-backed: the block reads through a bounded
    // window into a local holder (io.Address is a file offset there).
    // io.Address never dereferences on the web build.
    std::shared_ptr<std::vector<uint8_t>> artBlock;
    if (io.UopBlock != nullptr)
    {
        artBlock = vfs_asset_fetch_block_payload(
            io.UopBlock->Hash, io.UopBlock, true);
        if (artBlock == nullptr || artBlock->empty())
        {
            Warning(Data, "UOFileReader::GetArtPixels could not fetch art block %u", id);
            return {};
        }
    }
    else if (g_FileManager.m_ArtMul.FileBacked && g_FileManager.m_ArtMul.Loaded())
    {
        // client-era idx entries can understate the block size (the original
        // decoder over-read within the mapping - cursor 0x2073 is a known
        // case); window the read up to the file end (capped) so those blocks
        // still decode
        size_t want = io.DataSize;
        const auto fileOff = (size_t)io.Address;
        if (g_FileManager.m_ArtMul.Size > fileOff)
        {
            want = std::max<size_t>(
                want,
                std::min<size_t>(g_FileManager.m_ArtMul.Size - fileOff, 512 * 1024));
        }
        artBlock = std::make_shared<std::vector<uint8_t>>(want, 0);
        g_FileManager.m_ArtMul.Seek(io.Address);
        g_FileManager.m_ArtMul.ReadDataLE(artBlock->data(), want);
    }
    else
    {
        return {};
    }
    uint16_t *P = (uint16_t *)artBlock->data();
#else
    // native: io.Address must land inside the backing mapped file (art.mul or
    // artLegacyMUL.uop). A corrupt/streamed idx entry bakes a wild pointer:
    // dereferencing it segfaults outside every vfs trap range (unrecoverable,
    // the trap handler re-raises foreign faults). Degrade to a missing sprite.
    {
        const auto inMapping = [](const CMappedFile &file, const CIndexObject &obj)
        {
            const auto *addr = reinterpret_cast<const uint8_t *>(obj.Address);
            return file.Start != nullptr && addr >= file.Start &&
                   (addr - file.Start) + obj.DataSize <= file.Size;
        };
        if (
            !inMapping(g_FileManager.m_ArtMul, io) &&
            !inMapping(g_FileManager.m_ArtLegacyMUL, io))
        {
            Warning(
                Data,
                "UOFileReader::GetArtPixels art %u block out of range "
                "(addr %llx, size %u, art [%p..%p], uop [%p..%p])",
                id,
                (unsigned long long)io.Address,
                io.DataSize,
                (const void *)g_FileManager.m_ArtMul.Start,
                (const void *)(g_FileManager.m_ArtMul.Start + g_FileManager.m_ArtMul.Size),
                (const void *)g_FileManager.m_ArtLegacyMUL.Start,
                (const void *)(g_FileManager.m_ArtLegacyMUL.Start +
                               g_FileManager.m_ArtLegacyMUL.Size));
            return {};
        }
    }
    uint16_t *P = (uint16_t *)io.Address;
#endif
    uint16_t color = io.Color;
    std::vector<uint16_t> pixels;
    if (!run) //raw tile
    {
        if (io.DataSize < 44 * 22 * 2)
        {
            Warning(Data, "UOFileReader::GetArtPixels land block %i too small", id);
            return pixels;
        }
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
#if defined(XUO_EMSC)
            // stored run tiles: [u16 width][u16 height][line offsets...]
            uint16_t *ptr = reinterpret_cast<uint16_t *>(artBlock->data() + 4);
#else
            uint16_t *ptr = (uint16_t *)(io.Address + 4);
#endif
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
#if defined(XUO_EMSC)
            // web: io.Address is a file offset, the block lives in the
            // fetched window - bounds are the window itself
            const uint8_t *blockStart = artBlock->data();
            const uint8_t *blockEnd = artBlock->data() + artBlock->size();
#else
            const uint8_t *blockStart = (const uint8_t *)io.Address;
            // client-era idx entries can understate the block size; the
            // original decoder silently over-read within the mapping. Clamp
            // the readable range to the containing file instead of trusting
            // the (possibly wrong) index size - still bounded, never faults
            const uint8_t *blockEnd = blockStart + io.DataSize;
            const CMappedFile *artFiles[] = {
                (const CMappedFile *)&g_FileManager.m_ArtMul,
                (const CMappedFile *)&g_FileManager.m_ArtLegacyMUL
            };
            for (const CMappedFile *f : artFiles)
            {
                const auto *addr = (const uint8_t *)io.Address;
                if (f->Start != nullptr && addr >= f->Start && addr < f->Start + f->Size)
                {
                    blockEnd = std::max(
                        blockEnd, (const uint8_t *)f->Start + f->Size);
                }
            }
#endif
            auto readOutOfBounds = [&blockStart, &blockEnd](const void *p, size_t size)
            {
                return p < blockStart || (const uint8_t *)p + size > blockEnd;
            };
            if (readOutOfBounds(dataStart, 2))
            {
                Warning(
                    Data,
                    "UOFileReader::ReadArt art block %i truncated (data size: %u)",
                    id,
                    io.DataSize);
                return pixels;
            }
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

            auto lineOffsetOutOfBounds =
                [&lineOffsets, &dataStart, &readOutOfBounds](int y)
            {
                return readOutOfBounds(&lineOffsets[y], 2) ||
                       readOutOfBounds(dataStart + (lineOffsets[y] * 2), 4);
            };
            if (lineOffsetOutOfBounds(0))
            {
                Warning(
                    Data,
                    "UOFileReader::ReadArt art block %i line offset out of bounds",
                    id);
                pixels.clear();
                return pixels;
            }
            ptr = (uint16_t *)(dataStart + (lineOffsets[0] * 2));
            while (Y < height)
            {
                if (readOutOfBounds(ptr, 4))
                {
                    Warning(Data, "UOFileReader::ReadArt art block %i run out of bounds", id);
                    pixels.clear();
                    return pixels;
                }
                XOffs = *ptr;
                ptr++;
                Run = *ptr;
                ptr++;
                if (XOffs + Run >= 2048 || X + XOffs + Run > width ||
                    readOutOfBounds(ptr, Run * 2))
                {
                    Warning(
                        Data,
                        "UOFileReader::ReadArt bad offset:%i, %i (x:%i, width:%i)",
                        XOffs,
                        Run,
                        X,
                        width);
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
                    if (Y < height)
                    {
                        if (lineOffsetOutOfBounds(Y))
                        {
                            Warning(
                                Data,
                                "UOFileReader::ReadArt art block %i line offset out of bounds",
                                id);
                            pixels.clear();
                            return pixels;
                        }
                        ptr = (uint16_t *)(dataStart + (lineOffsets[Y] * 2));
                    }
                }
            }

            if ((id >= 0x2053 && id <= 0x2062) ||
                (id >= 0x206A && id <= 0x2079)) // remove the border (these are mouse cursor gumps)
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
                val = g_ColorManager.GetColor16(val, color);
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

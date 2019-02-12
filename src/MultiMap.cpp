// MIT License
// Copyright (C) August 2016 Hotride

#include "MultiMap.h"
#include "Logging.h"
#include "Gumps/GumpMap.h"
#include "Managers/ColorManager.h"
#include "Managers/FileManager.h"
#include "Wisp/WispMappedFile.h"
#include "Wisp/WispDataStream.h"

CMultiMap g_MultiMap;

CMultiMap::CMultiMap()
{
}

CMultiMap::~CMultiMap()
{
}

void CMultiMap::LoadMap(CGumpMap *gump, CGUIExternalTexture *mapObject)
{
    DEBUG_TRACE_FUNCTION;
    Wisp::CMappedFile &file = g_FileManager.m_MultiMap;

    if (file.Size == 0u)
    {
        Warning(Data, "MultiMap.rle is not loaded!");
        return;
    }

    file.ResetPtr();

    int Width = file.ReadInt32LE();
    int Height = file.ReadInt32LE();

    if (Width < 1 || Height < 1)
    {
        Warning(Data, "Failed to load bounds from MultiMap.rle");
        return;
    }

    int mapSize = gump->Width * gump->Height;
    vector<uint8_t> byteMap(mapSize, 0);

    int startX = gump->StartX / 2;
    int endX = gump->EndX / 2;

    int widthDivizor = endX - startX;

    if (widthDivizor == 0)
    {
        widthDivizor++;
    }

    int startY = gump->StartY / 2;
    int endY = gump->EndY / 2;

    int heightDivizor = endY - startY;

    if (heightDivizor == 0)
    {
        heightDivizor++;
    }

    int width = (gump->Width << 8) / widthDivizor;
    int height = (gump->Height << 8) / heightDivizor;

    int x = 0;
    int y = 0;

    int maxPixelValue = 1;

    while (!file.IsEOF())
    {
        uint8_t pic = file.ReadUInt8();
        uint8_t size = pic & 0x7F;

        bool colored = (pic & 0x80) != 0u;

        int startHeight = startY * height;
        int currentHeight = y * height;
        int posY = gump->Width * ((currentHeight - startHeight) >> 8);

        for (int i = 0; i < size; i++)
        {
            if (colored && x >= startX && x < endX && y >= startY && y < endY)
            {
                int position = posY + ((width * (x - startX)) >> 8);

                uint8_t &pixel = byteMap[position];

                if (pixel < 0xFF)
                {
                    if (pixel == maxPixelValue)
                    {
                        maxPixelValue++;
                    }

                    pixel++;
                }
            }

            x++;

            if (x >= Width)
            {
                x = 0;
                y++;
                currentHeight += height;
                posY = gump->Width * ((currentHeight - startHeight) >> 8);
            }

            //if (y >= Height)
            //	break;
        }

        //if (y >= Height)
        //	break;
    }

    if (maxPixelValue >= 1)
    {
        uint16_t *huesData =
            (uint16_t *)((uint8_t *)g_ColorManager.GetHuesRangePointer() + 30800); // color = 0x015C

        vector<uint16_t> colorTable(maxPixelValue);
        int colorOffset = 31 * maxPixelValue;

        for (int i = 0; i < maxPixelValue; i++)
        {
            colorOffset -= 31;
            colorTable[i] = 0x8000 | huesData[colorOffset / maxPixelValue];
        }

        vector<uint16_t> wordMap(mapSize);

        for (int i = 0; i < mapSize; i++)
        {
            uint8_t &pic = byteMap[i];

            wordMap[i] = (pic != 0u ? colorTable[pic - 1] : 0);
        }

        g_GL_BindTexture16(*mapObject->m_Texture, gump->Width, gump->Height, &wordMap[0]);
    }
}

bool CMultiMap::LoadFacet(CGumpMap *gump, CGUIExternalTexture *mapObject, int facet)
{
    DEBUG_TRACE_FUNCTION;
    if (facet < 0 || facet > 5)
    {
        Warning(Data, "Invalid facet index: %i", facet);
        return false;
    }

    Wisp::CMappedFile &file = g_FileManager.m_FacetMul[facet];
    file.ResetPtr();

    if (file.Size == 0u)
    {
        Warning(Data, "Facet %i is not loaded!", facet);
        return false;
    }

    int mapWidth = file.ReadInt16LE();
    int mapHeight = file.ReadInt16LE();

    int startX = gump->StartX;
    int endX = gump->EndX;

    int startY = gump->StartY;
    int endY = gump->EndY;

    int width = endX - startX;
    int height = endY - startY;

    vector<uint16_t> map(width * height);

    for (int y = 0; y < mapHeight; y++)
    {
        int x = 0;
        int colorCount = file.ReadInt32LE() / 3;

        for (int i = 0; i < colorCount; i++)
        {
            int size = file.ReadUInt8();
            uint16_t color = 0x8000 | file.ReadInt16LE();

            for (int j = 0; j < size; j++)
            {
                if ((x >= startX && x < endX) && (y >= startY && y < endY))
                {
                    map[((y - startY) * width) + (x - startX)] = color;
                }

                x++;
            }
        }
    }

    g_GL_BindTexture16(*mapObject->m_Texture, width, height, &map[0]);

    //Или не надо...?
    mapObject->m_Texture->Width = gump->Width;
    mapObject->m_Texture->Height = gump->Height;

    return true;
}

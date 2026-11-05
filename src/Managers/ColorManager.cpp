// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "ColorManager.h"
#include <cstdint>
#include <xuocore/uodata.h>
#include <xuocore/uodata_reader.h> // uo_color16_to_32, uo_hue_get_color16, uo_radar_color
#include "../Renderer/RenderAPI.h"
#include "../Globals.h" // ToColor*, g_ShaderColorTableInUse, SPECTRAL_COLOR_FLAG
#include "Constants.h"

#define RENDERER_INTERNAL
#include "../Renderer/RenderInternal.h"

CColorManager g_ColorManager;

void CColorManager::Init()
{
    // radarcol.mul is read on demand by uo_radar_color (see GetRadarColorData)
    g_ColorManager.CreateHuesPalette();
}

void CColorManager::CreateHuesPalette()
{
    m_HuesFloat.resize(GetHuesCount());
    const int entryCount = GetHuesCount() / 8;
    for (int i = 0; i < entryCount; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            auto &fh = m_HuesFloat[(i * 8) + j];
            for (int h = 0; h < 32; h++)
            {
                int idx = (int)h * 3;
                uint16_t c = g_FileManager.m_Hues[i].Entries[j].ColorTable[h];
                fh.Palette[idx] = (((c >> 10) & 0x1F) / 31.0f);
                fh.Palette[idx + 1] = (((c >> 5) & 0x1F) / 31.0f);
                fh.Palette[idx + 2] = ((c & 0x1F) / 31.0f);
            }
        }
    }
}

void CColorManager::SendColorsToShader(uint16_t color)
{
    if (!color || !GetHuesCount())
        return;

    float *palette = nullptr;
    if ((color & SPECTRAL_COLOR_FLAG) != 0)
    {
        palette = &m_HuesFloat[0].Palette[0];
    }
    else
    {
        if (color >= GetHuesCount())
        {
            color %= GetHuesCount();

            if (color == 0u)
            {
                color = 1;
            }
        }

        palette = &m_HuesFloat[color - 1].Palette[0];
    }

    assert(palette);
    SetColorPaletteCmd cmd{};
    memcpy(cmd.palette, palette, sizeof(cmd.palette));
    RenderAdd_SetColorPalette(g_renderCmdList, cmd);
}

uint32_t CColorManager::Color16To32(uint16_t c) const
{
    return uo_color16_to_32(c);
}

uint16_t CColorManager::Color32To16(int c) const
{
    return (((c & 0xFF) * 32) / 256) | (((((c >> 16) & 0xff) * 32) / 256) << 10) |
           (((((c >> 8) & 0xff) * 32) / 256) << 5);
}

uint16_t CColorManager::ConvertToGray(uint16_t c) const
{
    return ((c & 0x1F) * 299 + ((c >> 5) & 0x1F) * 587 + ((c >> 10) & 0x1F) * 114) / 1000;
}

uint16_t CColorManager::GetColor16(uint16_t c, uint16_t color) const
{
    return uo_hue_get_color16(c, color);
}

uint16_t CColorManager::GetRadarColorData(int c) const
{
    return uo_radar_color(c);
}

uint32_t CColorManager::GetPolygoneColor(uint16_t c, uint16_t color) const
{
    int g = 0;
    int e = 0;
    if (uo_hue_index(color, g, e))
    {
        return Color16To32(g_FileManager.m_Hues[g].Entries[e].ColorTable[c]);
    }
    return 0xFF010101; //Black
}

uint32_t CColorManager::GetUnicodeFontColor(uint16_t &c, uint16_t color) const
{
    int g = 0;
    int e = 0;
    if (uo_hue_index(color, g, e))
    {
        return Color16To32(g_FileManager.m_Hues[g].Entries[e].ColorTable[8]);
    }
    return Color16To32(c);
}

uint32_t CColorManager::GetColor(uint16_t &c, uint16_t color) const
{
    return uo_hue_partial_color16(c, color);
}

uint32_t CColorManager::GetPartialHueColor(uint16_t &c, uint16_t color) const
{
    const uint32_t cl = Color16To32(c);
    if (ToColorR(cl) == ToColorG(cl) && ToColorB(cl) == ToColorG(cl))
    {
        int g = 0;
        int e = 0;
        if (uo_hue_index(color, g, e))
        {
            return Color16To32(g_FileManager.m_Hues[g].Entries[e].ColorTable[(c >> 10) & 0x1F]);
        }
    }
    return cl;
}

uint16_t CColorManager::FixColor(uint16_t color, uint16_t defaultColor) const
{
    uint16_t fixedColor = color & 0x3FFF;
    if (fixedColor != 0u)
    {
        if (fixedColor >= 0x0BB8)
        {
            fixedColor = 1;
        }
        fixedColor |= (color & 0xC000);
    }
    else
    {
        fixedColor = defaultColor;
    }
    return fixedColor;
}

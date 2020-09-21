// MIT License
// Copyright (C) August 2016 Hotride

#include "ColorManager.h"
#include <xuocore/uodata.h>
#include "../Renderer/RenderAPI.h"
#include "../Globals.h" // ToColor*, g_ShaderColorTableInUse, SPECTRAL_COLOR_FLAG

CColorManager g_ColorManager;

void CColorManager::Init()
{
    if (g_FileManager.m_RadarcolMul.Size != 0)
    {
        m_Radarcol.resize(g_FileManager.m_RadarcolMul.Size / 2);
        memcpy(
            &m_Radarcol[0],
            (void *)g_FileManager.m_RadarcolMul.Start,
            g_FileManager.m_RadarcolMul.Size);
    }
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
    if (!color)
        return;

    //assert(g_ShaderColorTableInUse);
    if ((color & SPECTRAL_COLOR_FLAG) != 0)
    {
#ifndef NEW_RENDERER_ENABLED
        glUniform1fv(g_ShaderColorTableInUse, 32 * 3, &m_HuesFloat[0].Palette[0]);
#else
        RenderAdd_SetShaderLargeUniform(
            g_renderCmdList,
            ShaderLargeUniformCmd{ &m_HuesFloat[0].Palette[0],
                                   32 * 3,
                                   g_ShaderColorTableInUse,
                                   ShaderUniformType::ShaderUniformType_Float1V });
#endif
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

#ifndef NEW_RENDERER_ENABLED
        glUniform1fv(g_ShaderColorTableInUse, 32 * 3, &m_HuesFloat[color - 1].Palette[0]);
#else
        RenderAdd_SetShaderLargeUniform(
            g_renderCmdList,
            ShaderLargeUniformCmd{ &m_HuesFloat[color - 1].Palette[0],
                                   32 * 3,
                                   g_ShaderColorTableInUse,
                                   ShaderUniformType::ShaderUniformType_Float1V });
#endif
    }
}

uint32_t CColorManager::Color16To32(uint16_t c) const
{
    const uint8_t table[32] = { 0x00, 0x08, 0x10, 0x18, 0x20, 0x29, 0x31, 0x39, 0x41, 0x4A, 0x52,
                                0x5A, 0x62, 0x6A, 0x73, 0x7B, 0x83, 0x8B, 0x94, 0x9C, 0xA4, 0xAC,
                                0xB4, 0xBD, 0xC5, 0xCD, 0xD5, 0xDE, 0xE6, 0xEE, 0xF6, 0xFF };

    return (table[(c >> 10) & 0x1F] | (table[(c >> 5) & 0x1F] << 8) | (table[c & 0x1F] << 16));
    /*return
	(
		(((c >> 10) & 0x1F) * 0xFF / 0x1F) |
		((((c >> 5) & 0x1F) * 0xFF / 0x1F) << 8) |
		(((c & 0x1F) * 0xFF / 0x1F) << 16)
	);*/
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
    if (color != 0 && color < GetHuesCount())
    {
        color -= 1;
        const int g = color / 8;
        const int e = color % 8;
        return g_FileManager.m_Hues[g].Entries[e].ColorTable[(c >> 10) & 0x1F];
    }
    return c;
}

uint16_t CColorManager::GetRadarColorData(int c) const
{
    if (c < (int)m_Radarcol.size())
    {
        return m_Radarcol[c];
    }
    return 0;
}

uint32_t CColorManager::GetPolygoneColor(uint16_t c, uint16_t color) const
{
    if (color != 0 && color < GetHuesCount())
    {
        color -= 1;
        const int g = color / 8;
        const int e = color % 8;
        return Color16To32(g_FileManager.m_Hues[g].Entries[e].ColorTable[c]);
    }
    return 0xFF010101; //Black
}

uint32_t CColorManager::GetUnicodeFontColor(uint16_t &c, uint16_t color) const
{
    if (color != 0 && color < GetHuesCount())
    {
        color -= 1;
        const int g = color / 8;
        const int e = color % 8;
        return Color16To32(g_FileManager.m_Hues[g].Entries[e].ColorTable[8]);
    }
    return Color16To32(c);
}

uint32_t CColorManager::GetColor(uint16_t &c, uint16_t color) const
{
    if (color != 0 && color < GetHuesCount())
    {
        color -= 1;
        const int g = color / 8;
        const int e = color % 8;
        return Color16To32(g_FileManager.m_Hues[g].Entries[e].ColorTable[(c >> 10) & 0x1F]);
    }
    return Color16To32(c);
}

uint32_t CColorManager::GetPartialHueColor(uint16_t &c, uint16_t color) const
{
    if (color != 0 && color < GetHuesCount())
    {
        color -= 1;
        const int g = color / 8;
        const int e = color % 8;
        const uint32_t cl = Color16To32(c);
        if (ToColorR(cl) == ToColorG(cl) && ToColorB(cl) == ToColorG(cl))
        {
            return Color16To32(g_FileManager.m_Hues[g].Entries[e].ColorTable[(c >> 10) & 0x1F]);
        }
    }
    return Color16To32(c);
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

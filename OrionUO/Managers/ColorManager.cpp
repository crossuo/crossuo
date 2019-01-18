// MIT License
// Copyright (C) August 2016 Hotride

#include "ColorManager.h"
#include "FileManager.h"

CColorManager g_ColorManager;

CColorManager::CColorManager()
    : m_HuesCount(0)
{
}

CColorManager::~CColorManager()
{
}

void CColorManager::Init()
{
    DEBUG_TRACE_FUNCTION;
    intptr_t addr = (intptr_t)g_FileManager.m_HuesMul.Start;
    size_t size = g_FileManager.m_HuesMul.Size;

    if (addr > 0 && size > 0 && addr != -1 && size != -1)
    {
        size_t entryCount = size / sizeof(HUES_GROUP);

        m_HuesCount = (int)entryCount * 8;
        m_HuesRange.resize(entryCount);

        memcpy(&m_HuesRange[0], (void *)addr, entryCount * sizeof(HUES_GROUP));
    }
    else
    {
        m_HuesCount = 0;
    }

    if (g_FileManager.m_RadarcolMul.Size != 0u)
    {
        m_Radarcol.resize(g_FileManager.m_RadarcolMul.Size / 2);
        memcpy(
            &m_Radarcol[0],
            (void *)g_FileManager.m_RadarcolMul.Start,
            g_FileManager.m_RadarcolMul.Size);
    }
}

void CColorManager::SetHuesBlock(int index, VERDATA_HUES_GROUP *group)
{
    DEBUG_TRACE_FUNCTION;
    if (index < 0 || index >= m_HuesCount)
    {
        return;
    }

    m_HuesRange[index].Header = group->Header;
    for (int i = 0; i < 8; i++)
    {
        memcpy(
            &m_HuesRange[index].Entries[i].ColorTable[0],
            &group->Entries[i].ColorTable[0],
            sizeof(uint16_t[32]));
    }
}

void CColorManager::CreateHuesPalette()
{
    DEBUG_TRACE_FUNCTION;
    m_HuesFloat.resize(m_HuesCount);
    int entryCount = m_HuesCount / 8;

    for (int i = 0; i < entryCount; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            FLOAT_HUES &fh = m_HuesFloat[(i * 8) + j];

            for (int h = 0; h < 32; h++)
            {
                int idx = (int)h * 3;

                uint16_t c = m_HuesRange[i].Entries[j].ColorTable[h];

                fh.Palette[idx] = (((c >> 10) & 0x1F) / 31.0f);
                fh.Palette[idx + 1] = (((c >> 5) & 0x1F) / 31.0f);
                fh.Palette[idx + 2] = ((c & 0x1F) / 31.0f);
            }
        }
    }
}

void CColorManager::SendColorsToShader(uint16_t color)
{
    DEBUG_TRACE_FUNCTION;
    if (color != 0)
    {
        if ((color & SPECTRAL_COLOR_FLAG) != 0)
        {
            glUniform1fv(ShaderColorTable, 32 * 3, &m_HuesFloat[0].Palette[0]);
        }
        else
        {
            if (color >= m_HuesCount)
            {
                color %= m_HuesCount;

                if (color == 0u)
                {
                    color = 1;
                }
            }

            glUniform1fv(ShaderColorTable, 32 * 3, &m_HuesFloat[color - 1].Palette[0]);
        }
    }
}

uint32_t CColorManager::Color16To32(uint16_t c)
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

uint16_t CColorManager::Color32To16(int c)
{
    return (((c & 0xFF) * 32) / 256) | (((((c >> 16) & 0xff) * 32) / 256) << 10) |
           (((((c >> 8) & 0xff) * 32) / 256) << 5);
}

uint16_t CColorManager::ConvertToGray(uint16_t c)
{
    return ((c & 0x1F) * 299 + ((c >> 5) & 0x1F) * 587 + ((c >> 10) & 0x1F) * 114) / 1000;
}

uint16_t CColorManager::GetColor16(uint16_t c, uint16_t color)
{
    DEBUG_TRACE_FUNCTION;
    if (color != 0 && color < m_HuesCount)
    {
        color -= 1;
        int g = color / 8;
        int e = color % 8;

        return m_HuesRange[g].Entries[e].ColorTable[(c >> 10) & 0x1F];
    }

    return c;
}

uint16_t CColorManager::GetRadarColorData(int c)
{
    DEBUG_TRACE_FUNCTION;
    if (c < (int)m_Radarcol.size())
    {
        return m_Radarcol[c];
    }

    return 0;
}

uint32_t CColorManager::GetPolygoneColor(uint16_t c, uint16_t color)
{
    DEBUG_TRACE_FUNCTION;
    if (color != 0 && color < m_HuesCount)
    {
        color -= 1;
        int g = color / 8;
        int e = color % 8;

        return Color16To32(m_HuesRange[g].Entries[e].ColorTable[c]);
    }

    return 0xFF010101; //Black
}

uint32_t CColorManager::GetUnicodeFontColor(uint16_t &c, uint16_t color)
{
    DEBUG_TRACE_FUNCTION;
    if (color != 0 && color < m_HuesCount)
    {
        color -= 1;
        int g = color / 8;
        int e = color % 8;

        return Color16To32(m_HuesRange[g].Entries[e].ColorTable[8]);
    }

    return Color16To32(c);
}

uint32_t CColorManager::GetColor(uint16_t &c, uint16_t color)
{
    DEBUG_TRACE_FUNCTION;
    if (color != 0 && color < m_HuesCount)
    {
        color -= 1;
        int g = color / 8;
        int e = color % 8;

        return Color16To32(m_HuesRange[g].Entries[e].ColorTable[(c >> 10) & 0x1F]);
    }

    return Color16To32(c);
}

uint32_t CColorManager::GetPartialHueColor(uint16_t &c, uint16_t color)
{
    DEBUG_TRACE_FUNCTION;
    if (color != 0 && color < m_HuesCount)
    {
        color -= 1;
        int g = color / 8;
        int e = color % 8;

        uint32_t cl = Color16To32(c);

        if (ToColorR(cl) == ToColorG(cl) && ToColorB(cl) == ToColorG(cl))
        {
            return Color16To32(m_HuesRange[g].Entries[e].ColorTable[(c >> 10) & 0x1F]);
        }

        return cl;
    }

    return Color16To32(c);
}

uint16_t CColorManager::FixColor(uint16_t color, uint16_t defaultColor)
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

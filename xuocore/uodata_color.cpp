// SPDX-License-Identifier: AGPL-3.0-or-later

// Color utilities
#include "uodata_color.h"
#include "uodata.h"
#include <mutex>

uint32_t uo_color16_to_32(uint16_t c)
{
    // the 5-bit expansion table the client ships (perceptually uniform over
    // the 32 RGB555 levels)
    static const uint8_t table[32] = { 0x00, 0x08, 0x10, 0x18, 0x20, 0x29, 0x31, 0x39,
                                       0x41, 0x4A, 0x52, 0x5A, 0x62, 0x6A, 0x73, 0x7B,
                                       0x83, 0x8B, 0x94, 0x9C, 0xA4, 0xAC, 0xB4, 0xBD,
                                       0xC5, 0xCD, 0xD5, 0xDE, 0xE6, 0xEE, 0xF6, 0xFF };
    return (table[(c >> 10) & 0x1F] | (table[(c >> 5) & 0x1F] << 8) | (table[c & 0x1F] << 16));
}

uint16_t uo_color32_to_16(int c)
{
    return (((c & 0xFF) * 32) / 256) | (((((c >> 16) & 0xff) * 32) / 256) << 10) |
           (((((c >> 8) & 0xff) * 32) / 256) << 5);
}

uint16_t uo_color_to_gray(uint16_t c)
{
    return ((c & 0x1F) * 299 + ((c >> 5) & 0x1F) * 587 + ((c >> 10) & 0x1F) * 114) / 1000;
}

bool uo_hue_index(uint16_t color, int &group, int &entry)
{
    if (color != 0u && (size_t)color < g_FileManager.m_Hues.size())
    {
        color -= 1;
        group = color / 8;
        entry = color % 8;
        return true;
    }
    return false;
}

uint16_t uo_hue_get_color16(uint16_t c, uint16_t color)
{
    int g = 0;
    int e = 0;
    if (uo_hue_index(color, g, e))
    {
        return g_FileManager.m_Hues[g].Entries[e].ColorTable[(c >> 10) & 0x1F];
    }
    return c;
}

uint32_t uo_hue_partial_color16(uint16_t c, uint16_t color)
{
    int g = 0;
    int e = 0;
    if (uo_hue_index(color, g, e))
    {
        // only grey (R == G == B) pixels take the hue, in RGB555 terms
        const uint16_t r = uint16_t((c >> 10) & 0x1F);
        const uint16_t gg = uint16_t((c >> 5) & 0x1F);
        const uint16_t b = uint16_t(c & 0x1F);
        if (r == gg && b == gg)
        {
            return g_FileManager.m_Hues[g].Entries[e].ColorTable[r];
        }
    }
    return 0;
}

uint32_t uo_hue_get_color_poly(uint16_t c, uint16_t color)
{
    int g = 0;
    int e = 0;
    if (uo_hue_index(color, g, e))
    {
        return uo_color16_to_32(g_FileManager.m_Hues[g].Entries[e].ColorTable[c]);
    }
    return 0xFF010101; // black. should it be uo_color16_to_32(c) ? // was GetPolygoneColor
}

uint16_t uo_radar_color(int id)
{
    static std::mutex s_lock;
    static std::vector<uint16_t> s_table;
    static bool s_loaded = false;
    {
        std::lock_guard<std::mutex> guard(s_lock);
        if (!s_loaded)
        {
            s_loaded = true;
            CMappedFile &file = g_FileManager.m_RadarcolMul;
            const size_t size = file.Size;
            if (size > 1)
            {
                s_table.resize(size / 2);
                // one large read (mapped copy on native, bounded reads on
                // web) exactly like the client's color manager does
                file.Seek(0);
                file.ReadDataLE((uint8_t *)s_table.data(), size);
            }
        }
    }
    if (id >= 0 && (size_t)id < s_table.size())
    {
        return s_table[(size_t)id];
    }
    return 0;
}

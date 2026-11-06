// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once
#include <stdint.h>
#include <xuocore/mulstruct.h> // HUES_GROUP
#include <xuocore/uodata.h>    // g_FileManager

struct FLOAT_HUES
{
    float Palette[32 * 3];
};

struct CColorManager
{
    std::vector<FLOAT_HUES> m_HuesFloat;

    void Init();

    inline HUES_GROUP *GetHuesRangePointer() { return &g_FileManager.m_Hues[0]; }
    inline int GetHuesCount() const { return (int)g_FileManager.m_Hues.size(); }
    void CreateHuesPalette();
    void SendColorsToShader(uint16_t color);
    uint32_t GetPartialHueColor(uint16_t &c, uint16_t color) const;
    uint16_t FixColor(uint16_t color, uint16_t defaultColor = 0) const;
};

extern CColorManager g_ColorManager;

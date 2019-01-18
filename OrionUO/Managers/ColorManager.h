// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../plugin/mulstruct.h"

struct FLOAT_HUES
{
    float Palette[32 * 3];
};

class CColorManager
{
private:
    vector<HUES_GROUP> m_HuesRange;
    vector<FLOAT_HUES> m_HuesFloat;
    int m_HuesCount{ 0 };
    vector<uint16_t> m_Radarcol;

public:
    CColorManager();
    ~CColorManager();

    void Init();

    HUES_GROUP *GetHuesRangePointer() { return &m_HuesRange[0]; }
    int GetHuesCount() const { return m_HuesCount; }
    void SetHuesBlock(int index, VERDATA_HUES_GROUP *group);
    void CreateHuesPalette();
    void SendColorsToShader(uint16_t color);
    uint16_t Color32To16(int c);
    uint32_t Color16To32(uint16_t c);
    uint16_t ConvertToGray(uint16_t c);
    uint16_t GetColor16(uint16_t c, uint16_t color);
    uint16_t GetRadarColorData(int c);
    uint32_t GetPolygoneColor(uint16_t c, uint16_t color);
    uint32_t GetUnicodeFontColor(uint16_t &c, uint16_t color);
    uint32_t GetColor(uint16_t &c, uint16_t color);
    uint32_t GetPartialHueColor(uint16_t &c, uint16_t color);
    uint16_t FixColor(uint16_t color, uint16_t defaultColor = 0);
};

extern CColorManager g_ColorManager;

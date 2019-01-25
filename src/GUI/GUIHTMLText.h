// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseGUI.h"

class CGUIHTMLText : public CBaseGUI
{
public:
    uint32_t TextID = 0;
    uint32_t HTMLStartColor = 0;
    wstring Text = {};
    uint8_t Font = 0;
    TEXT_ALIGN_TYPE Align = TS_LEFT;
    uint16_t TextFlags = 0;
    int Width = 0;

    CGUIHTMLText(
        int index,
        uint8_t font,
        uint16_t color,
        int x,
        int y,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t textFlags = 0,
        int htmlStartColor = 0xFFFFFFFF);
    virtual ~CGUIHTMLText();

    CGLHTMLTextTexture m_Texture{ CGLHTMLTextTexture() };

    virtual CSize GetSize() { return CSize(m_Texture.Width, m_Texture.Height); }
    void CreateTexture(bool backgroundCanBeColored);

    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};

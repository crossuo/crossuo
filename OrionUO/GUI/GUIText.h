// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGUIText : public CBaseGUI
{
public:
    CGUIText(uint16_t color, int x, int y);
    virtual ~CGUIText();

    CGLTextTexture m_Texture{ CGLTextTexture() };

    virtual Wisp::CSize GetSize() { return Wisp::CSize(m_Texture.Width, m_Texture.Height); }

    void CreateTextureA(
        uint8_t font,
        const string &str,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);

    void CreateTextureW(
        uint8_t font,
        const wstring &str,
        uint8_t cell = 30,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);

    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};

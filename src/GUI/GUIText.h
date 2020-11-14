// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <common/str.h>
#include "BaseGUI.h"
#include "Sprite.h"

class CGUIText : public CBaseGUI
{
public:
    CGUIText(uint16_t color, int x, int y);
    virtual ~CGUIText();

    CTextSprite m_Texture;

    virtual CSize GetSize() { return CSize(m_Texture.Width, m_Texture.Height); }

    void CreateTextureA(
        uint8_t font,
        const astr_t &str,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);

    void CreateTextureW(
        uint8_t font,
        const wstr_t &str,
        uint8_t cell = 30,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);

    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};

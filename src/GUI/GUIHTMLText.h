// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <common/str.h>
#include "BaseGUI.h"
#include "../Sprite.h"

class CGUIHTMLText : public CBaseGUI
{
public:
    uint32_t TextID = 0;
    uint32_t HTMLStartColor = 0;
    wstr_t Text = {};
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

    CHTMLText m_Sprite;

    virtual CSize GetSize() { return CSize(m_Sprite.Width, m_Sprite.Height); }
    void Create(bool backgroundCanBeColored);

    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};

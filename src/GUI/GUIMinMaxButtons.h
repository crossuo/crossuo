// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "BaseGUI.h"
#include "Sprite.h"

class CGUIMinMaxButtons : public CBaseGUI
{
public:
    //! Minimum value
    int MinValue = 0;

    //! Maximum value
    int MaxValue = 0;

    //! Current value
    int Value = 0;

    //! Has text
    bool HaveText = false;

    //! Text position
    SLIDER_TEXT_POSITION TextPosition = STP_RIGHT;

    //! Text font
    uint8_t Font = 0;

    //! Text color
    uint16_t TextColor = 0;

    //! Unicode text
    bool Unicode = true;

    //! Text width
    int TextWidth = 0;

    //! Text orientation
    TEXT_ALIGN_TYPE Align = TS_LEFT;

    //! Text flags
    uint16_t TextFlags = 0;

    //! Text X coordinate
    int TextX = 0;

    //! Text Y coordinate
    int TextY = 0;

    //! Base offset step
    int BaseScrollStep = 1;

    //! Current offset step
    int ScrollStep = 1;

    //! Time of the last scroll iteration
    uint32_t LastScrollTime = 0;

    //! Default text offset
    int DefaultTextOffset = 2;

private:
    //! Text texture
    CTextSprite Text;

    //! Scroll method (NoScroll/Max/Min)
    int m_ScrollMode{ 0 };

public:
    CGUIMinMaxButtons(
        int serial, uint16_t graphic, int x, int y, int minValue, int maxValue, int value);
    virtual ~CGUIMinMaxButtons();

    virtual bool IsPressedOuthit() { return true; }

    virtual CSize GetSize() { return CSize(36, 18); }

    // Scroll
    virtual void Scroll(int delay);

    // Press
    virtual void OnClick();

    //! Update the text
    void UpdateText();

    //! Set the text parameters
    void SetTextParameters(
        bool haveText,
        SLIDER_TEXT_POSITION textPosition,
        uint8_t font,
        uint16_t color,
        bool unicode,
        int textWidth = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t textFlags = 0);

    virtual void PrepareTextures();

    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};

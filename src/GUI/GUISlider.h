// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "BaseGUI.h"
#include "Sprite.h"

class CGUISlider : public CBaseGUI
{
public:
    uint16_t GraphicSelected = 0;
    uint16_t GraphicPressed = 0;
    uint16_t BackgroundGraphic = 0;
    bool CompositeBackground = false;
    bool Vertical = false;
    int Length = 0;
    float Percents = 0.0f;
    int Offset = 0;
    int MinValue = 0;
    int MaxValue = 0;
    int Value = 0;
    bool HaveText = false;
    SLIDER_TEXT_POSITION TextPosition = STP_RIGHT;
    uint8_t Font = 0;
    uint16_t TextColor = 0;
    bool Unicode = true;
    int TextWidth = 0;
    TEXT_ALIGN_TYPE Align = TS_LEFT;
    uint16_t TextFlags = 0;
    int TextX = 0;
    int TextY = 0;
    int ScrollStep = 15;
    uint32_t LastScrollTime = 0;
    int DefaultTextOffset = 2;

private:
    CTextSprite Text;

public:
    CGUISlider(
        int serial,
        uint16_t graphic,
        uint16_t graphicSelected,
        uint16_t graphicPressed,
        uint16_t backgroundGraphic,
        bool compositeBackground,
        bool vertical,
        int x,
        int y,
        int length,
        int minValue,
        int maxValue,
        int value);
    virtual ~CGUISlider();

    virtual bool IsPressedOuthit() { return true; }
    virtual CSize GetSize();
    virtual void OnScroll(bool up, int delay);
    virtual void OnClick(int x, int y);
    void UpdateText();
    virtual void CalculateOffset();
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
    virtual uint16_t GetDrawGraphic();
    virtual void Draw(bool checktrans = false);
    virtual bool Select();
    virtual void OnMouseEnter();
    virtual void OnMouseExit();
};

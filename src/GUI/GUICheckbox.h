// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <common/str.h>
#include <xuocore/enumlist.h>
#include "GUIDrawObject.h"
#include "Sprite.h"

class CGUICheckbox : public CGUIDrawObject
{
public:
    //! Picture ID in the pressed state
    uint16_t GraphicChecked = 0;

    //! Picture ID in the checked state
    uint16_t GraphicSelected = 0;

    //! Picture ID in the disabled state
    uint16_t GraphicDisabled = 0;

    //! Component state
    bool Checked = false;

    //! Text position
    SLIDER_TEXT_POSITION TextPosition = STP_RIGHT;

    //! Text X coordinate
    int TextX = 0;

    //! Text Y coordinate
    int TextY = 0;

    //! Default text offset
    int DefaultTextOffset = 2;

protected:
    CTextSprite Text;

    void UpdateTextPosition();

public:
    CGUICheckbox(
        int serial,
        uint16_t graphic,
        uint16_t graphicChecked,
        uint16_t graphicDisabled,
        int x,
        int y);
    virtual ~CGUICheckbox();

    //! Set the text
    void SetTextParameters(
        uint8_t font,
        const wstr_t &text,
        uint16_t color,
        SLIDER_TEXT_POSITION textPosition = STP_RIGHT,
        int textWidth = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t textFlags = 0);
    void SetTextParameters(
        uint8_t font,
        const astr_t &text,
        uint16_t color,
        SLIDER_TEXT_POSITION textPosition = STP_RIGHT,
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

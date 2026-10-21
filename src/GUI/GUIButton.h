// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "GUIDrawObject.h"

class CGUIButton : public CGUIDrawObject
{
public:
    //! Picture ID for the mouse-hover state
    uint16_t GraphicSelected = 0;

    //! Picture ID for the pressed state
    uint16_t GraphicPressed = 0;

    //! Page index: -1 fires the gump's OnButton event with the button's ID; any other value switches pages without firing the event
    int ToPage = -1;

    //! Whether to process the button's pressed state on every render iteration
    bool ProcessPressedState = false;

    CGUIButton(
        int serial,
        uint16_t graphic,
        uint16_t graphicSelected,
        uint16_t graphicPressed,
        int x,
        int y);
    virtual ~CGUIButton();

    virtual void PrepareTextures();
    virtual uint16_t GetDrawGraphic();

    virtual void OnMouseEnter();
    virtual void OnMouseExit();
};

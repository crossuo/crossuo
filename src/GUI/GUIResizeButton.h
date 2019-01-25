// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUIButton.h"

class CGUIResizeButton : public CGUIButton
{
public:
    CGUIResizeButton(
        int serial,
        uint16_t graphic,
        uint16_t graphicSelected,
        uint16_t graphicPressed,
        int x,
        int y);
    virtual ~CGUIResizeButton();
    virtual bool IsPressedOuthit() { return true; }
};

// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUIDrawObject.h"

class CGUIGumppicHightlighted : public CGUIDrawObject
{
    uint16_t SelectedColor = 0;

public:
    CGUIGumppicHightlighted(
        int serial, uint16_t graphic, uint16_t color, uint16_t electedColor, int x, int y);
    virtual ~CGUIGumppicHightlighted();
    virtual void SetShaderMode();
    virtual void OnMouseEnter();
    virtual void OnMouseExit();
};

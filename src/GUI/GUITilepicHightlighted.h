// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUITilepic.h"

class CGUITilepicHightlighted : public CGUITilepic
{
    uint16_t SelectedColor = 0;
    bool DoubleDraw = false;

public:
    CGUITilepicHightlighted(
        int serial,
        uint16_t graphic,
        uint16_t color,
        uint16_t selectedColor,
        int x,
        int y,
        bool doubleDraw);
    virtual ~CGUITilepicHightlighted();

    virtual void SetShaderMode();

    virtual void Draw(bool checktrans = false);
    virtual bool Select();

    virtual void OnMouseEnter();
    virtual void OnMouseExit();
};

// MIT License
// Copyright (C) December 2016 Hotride

#pragma once

#include "GUITilepic.h"

class CGUITilepicScaled : public CGUITilepic
{
    int Width = 0;
    int Height = 0;

public:
    CGUITilepicScaled(uint16_t graphic, uint16_t color, int x, int y, int width, int height);
    virtual ~CGUITilepicScaled();

    virtual void Draw(bool checktrans = false);
    virtual bool Select() { return false; }
};

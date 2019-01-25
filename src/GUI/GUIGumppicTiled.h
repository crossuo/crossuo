// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUIDrawObject.h"

class CGUIGumppicTiled : public CGUIDrawObject
{
    int Width = 0;
    int Height = 0;

public:
    CGUIGumppicTiled(uint16_t graphic, int x, int y, int width, int height);
    virtual ~CGUIGumppicTiled();
    virtual CSize GetSize() { return CSize(Width, Height); }
    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};

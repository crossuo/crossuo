// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUIPolygonal.h"

class CGUIResizepic : public CGUIPolygonal
{
public:
    CGUIResizepic(int serial, uint16_t graphic, int x, int y, int width, int height);
    virtual ~CGUIResizepic();
    virtual CSize GetSize() { return CSize(Width, Height); }
    virtual void PrepareTextures();
    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};

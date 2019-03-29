// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUIPolygonal.h"

struct CGUIResizepic : public CGUIPolygonal
{
    virtual CSize GetSize() override { return CSize(Width, Height); }
    virtual void PrepareTextures() override;
    virtual bool Select() override;

    virtual void Draw(bool checktrans = false) override;

    CGUIResizepic(int serial, uint16_t graphic, int x, int y, int width, int height);
    virtual ~CGUIResizepic() = default;
};

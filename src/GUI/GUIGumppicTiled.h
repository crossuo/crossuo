// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUIDrawObject.h"

struct CGUIGumppicTiled : public CGUIDrawObject
{
    int Width = 0;
    int Height = 0;

    virtual CSize GetSize() override { return CSize(Width, Height); }
    virtual void Draw(bool checktrans = false) override;
    virtual bool Select() override;

    CGUIGumppicTiled(uint16_t graphic, int x, int y, int width, int height);
    virtual ~CGUIGumppicTiled() = default;
};

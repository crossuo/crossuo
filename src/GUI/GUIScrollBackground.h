// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseGUI.h"

struct CGUIScrollBackground : public CBaseGUI
{
    int Height = 0;
    int OffsetX = 0;
    int BottomOffsetX = 0;
    int Width = 0;
    CRect WorkSpace = CRect();

    void UpdateHeight(int height);
    virtual CSize GetSize() override { return CSize(Width, Height); }
    virtual void PrepareTextures() override;
    virtual bool Select() override;

    virtual void Draw(bool checktrans = false) override;

    CGUIScrollBackground(int serial, uint16_t graphic, int x, int y, int height);
    virtual ~CGUIScrollBackground() = default;
};

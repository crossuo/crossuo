// MIT License
// Copyright (C) September 2016 Hotride

#pragma once

#include "BaseGUI.h"

class CGUIWorldMapTexture : public CBaseGUI
{
public:
    int Index = 0;
    int Width = 0;
    int Height = 0;
    int OffsetX = 0;
    int OffsetY = 0;

    CGUIWorldMapTexture(int x, int y);
    virtual ~CGUIWorldMapTexture();
    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};

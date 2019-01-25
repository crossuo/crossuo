// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseGUI.h"

class CGUIDrawObject : public CBaseGUI
{
public:
    CGUIDrawObject(
        GUMP_OBJECT_TYPE type, int serial, uint16_t graphic, uint16_t color, int x, int y);
    virtual ~CGUIDrawObject();

    virtual CSize GetSize();

    virtual void SetShaderMode();
    virtual void PrepareTextures();

    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};

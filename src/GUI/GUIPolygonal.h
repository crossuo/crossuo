// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "BaseGUI.h"

struct CGUIPolygonal : public CBaseGUI
{
    int Width = 0;
    int Height = 0;
    bool CallOnMouseUp = false;

    virtual bool Select() override;
    virtual CSize GetSize() override { return CSize(Width, Height); }

    CGUIPolygonal(
        GUMP_OBJECT_TYPE type, int x, int y, int width, int height, bool callOnMouseUp = false);
    virtual ~CGUIPolygonal() = default;
};

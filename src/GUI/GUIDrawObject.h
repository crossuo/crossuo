// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "BaseGUI.h"

class CGUIDrawObject : public CBaseGUI
{
public:
    CGUIDrawObject(
        GUMP_OBJECT_TYPE type, int serial, uint16_t graphic, uint16_t color, int x, int y);
    virtual ~CGUIDrawObject() override;

    virtual CSize GetSize() override;

    virtual void SetShaderMode() override;
    virtual void PrepareTextures() override;

    virtual void Draw(bool checktrans = false) override;
    virtual bool Select() override;
};

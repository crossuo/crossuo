// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "GUITilepic.h"

struct CGUITilepicScaled : public CGUITilepic
{
    CGUITilepicScaled(uint16_t graphic, uint16_t color, int x, int y, int width, int height);
    virtual ~CGUITilepicScaled() override;

    virtual void Draw(bool checktrans = false) override;
    virtual bool Select() override { return false; }
};

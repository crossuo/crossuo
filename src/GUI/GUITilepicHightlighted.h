// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "GUITilepic.h"

class CGUITilepicHightlighted : public CGUITilepic
{
    uint16_t SelectedColor = 0;
    bool DoubleDraw = false;

public:
    CGUITilepicHightlighted(
        int serial,
        uint16_t graphic,
        uint16_t color,
        uint16_t selectedColor,
        int x,
        int y,
        bool doubleDraw);
    virtual ~CGUITilepicHightlighted() override;

    virtual void SetShaderMode() override;

    virtual void Draw(bool checktrans = false) override;
    virtual bool Select() override;

    virtual void OnMouseEnter() override;
    virtual void OnMouseExit() override;
};

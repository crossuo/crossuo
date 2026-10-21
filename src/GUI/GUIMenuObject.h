// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <common/str.h>
#include "GUITilepic.h"

class CGUIMenuObject : public CGUITilepic
{
    astr_t Text = "";

public:
    CGUIMenuObject(int serial, uint16_t graphic, uint16_t color, int x, int y, const astr_t &text);
    virtual ~CGUIMenuObject() override;

    virtual bool Select() override;

    virtual void OnMouseEnter() override;
    virtual void OnMouseExit() override;
};

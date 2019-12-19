// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include <common/str.h>
#include "GUITilepic.h"

class CGUIMenuObject : public CGUITilepic
{
    astr_t Text = "";

public:
    CGUIMenuObject(int serial, uint16_t graphic, uint16_t color, int x, int y, const astr_t &text);
    virtual ~CGUIMenuObject();

    virtual bool Select();

    virtual void OnMouseEnter();
    virtual void OnMouseExit();
};

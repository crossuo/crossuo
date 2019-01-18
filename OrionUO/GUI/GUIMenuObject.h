// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUITilepic.h"

class CGUIMenuObject : public CGUITilepic
{
    string Text = "";

public:
    CGUIMenuObject(int serial, uint16_t graphic, uint16_t color, int x, int y, const string &text);
    virtual ~CGUIMenuObject();

    virtual bool Select();

    virtual void OnMouseEnter();
    virtual void OnMouseExit();
};

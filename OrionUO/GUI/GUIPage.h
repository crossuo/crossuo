// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseGUI.h"

class CGUIPage : public CBaseGUI
{
public:
    int Index = 0;

    CGUIPage(int index);
    virtual ~CGUIPage();
};

// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseGUI.h"

class CGUIGroup : public CBaseGUI
{
public:
    //!Индекс группы. 0 - эквивалентно EndGroup
    int Index = 0;

    CGUIGroup(int index);
    virtual ~CGUIGroup();
};

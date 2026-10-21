// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "BaseGUI.h"

class CGUIGroup : public CBaseGUI
{
public:
    //! Group index. 0 is equivalent to EndGroup
    int Index = 0;

    CGUIGroup(int index);
    virtual ~CGUIGroup();
};

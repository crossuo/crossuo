﻿// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GUIGroup.h"

CGUIGroup::CGUIGroup(int index)
    : CBaseGUI(GOT_GROUP, 0, 0, 0, 0, 0)
    , Index(index)
{
}

CGUIGroup::~CGUIGroup()
{
}

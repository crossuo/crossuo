// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2017 Hotride

#include "ContainerStack.h"
#include <deque>

std::deque<CContainerStackItem> g_ContainerStack;
uint32_t g_CheckContainerStackTimer = 0;

CContainerStackItem::CContainerStackItem(
    int serial,
    short x,
    short y,
    short minimizedX,
    short minimizedY,
    bool minimized,
    bool lockMoving)
    : Serial(serial)
    , X(x)
    , Y(y)
    , MinimizedX(minimizedX)
    , MinimizedY(minimizedY)
    , Minimized(minimized)
    , LockMoving(lockMoving)
{
}

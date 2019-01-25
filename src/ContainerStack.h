// MIT License
// Copyright (C) September 2017 Hotride

#pragma once

class CContainerStackItem
{
public:
    uint32_t Serial = 0;
    short X = 0;
    short Y = 0;
    short MinimizedX = 0;
    short MinimizedY = 0;
    bool Minimized = false;
    bool LockMoving = false;

    CContainerStackItem(
        int serial,
        short x,
        short y,
        short minimizedX,
        short minimizedY,
        bool minimized,
        bool lockMoving);
    ~CContainerStackItem() {}
};

extern deque<CContainerStackItem> g_ContainerStack;

extern uint32_t g_CheckContainerStackTimer;

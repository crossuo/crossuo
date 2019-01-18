// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseQueue.h"

class CMulti : public CBaseQueueItem
{
public:
    short X = 0;
    short Y = 0;
    short MinX = 0;
    short MinY = 0;
    short MaxX = 0;
    short MaxY = 0;

    CMulti(short x, short y);
    virtual ~CMulti();
};

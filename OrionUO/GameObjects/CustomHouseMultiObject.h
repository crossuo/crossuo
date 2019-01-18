// MIT License
// Copyright (C) September 2017 Hotride

#pragma once

#include "MultiObject.h"

class CCustomHouseMultiObject : public CMultiObject
{
public:
    CCustomHouseMultiObject(uint16_t graphic, uint16_t color, short x, short y, char z, int flags);
    virtual ~CCustomHouseMultiObject();
    virtual bool IsCustomHouseMulti() { return true; }
};

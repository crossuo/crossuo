// MIT License
// Copyright (C) September 2017 Hotride

#include "CustomHouseMultiObject.h"

CCustomHouseMultiObject::CCustomHouseMultiObject(
    uint16_t graphic, uint16_t color, short x, short y, char z, int flags)
    : CMultiObject(graphic, x, y, z, flags)
{
    DEBUG_TRACE_FUNCTION;
    Color = color;
}

CCustomHouseMultiObject::~CCustomHouseMultiObject()
{
}

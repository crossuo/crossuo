// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2017 Hotride

#include "CustomHouseMultiObject.h"

CCustomHouseMultiObject::CCustomHouseMultiObject(
    uint16_t graphic, uint16_t color, short x, short y, char z, int flags)
    : CMultiObject(graphic, x, y, z, flags)
{
    Color = color;
}

CCustomHouseMultiObject::~CCustomHouseMultiObject()
{
}

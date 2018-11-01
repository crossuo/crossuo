// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** CustomHouseMultiObject.h
**
** Copyright (C) September 2017 Hotride
**
************************************************************************************
*/

#include "stdafx.h"

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


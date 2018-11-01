// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** GUITextEntry.cpp
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#include "stdafx.h"

CGUIGenericTextEntry::CGUIGenericTextEntry(
    int serial, int index, uint16_t color, int x, int y, int maxWidth, int maxLength)
    : CGUITextEntry(
          serial,
          color,
          color,
          color,
          x,
          y,
          maxWidth,
          true,
          (uint8_t)(g_PacketManager.GetClientVersion() >= CV_305D),
          TS_LEFT,
          UOFONT_BLACK_BORDER,
          maxLength)
    , TextID(index)
{
}

CGUIGenericTextEntry::~CGUIGenericTextEntry()
{
}


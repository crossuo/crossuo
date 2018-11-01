// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** GUIResizeButton.cpp
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#include "stdafx.h"

CGUIResizeButton::CGUIResizeButton(
    int serial, uint16_t graphic, uint16_t graphicSelected, uint16_t graphicPressed, int x, int y)
    : CGUIButton(serial, graphic, graphicSelected, graphicPressed, x, y)
{
    Type = GOT_RESIZEBUTTON;
}

CGUIResizeButton::~CGUIResizeButton()
{
}


// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** GUIHTMLResizepic.cpp
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#include "stdafx.h"

CGUIHTMLResizepic::CGUIHTMLResizepic(
    class CGUIHTMLGump *htmlGump, int serial, uint16_t graphic, int x, int y, int width, int height)
    : CGUIResizepic(serial, graphic, x, y, width, height)
    , m_HTMLGump(htmlGump)
{
}

CGUIHTMLResizepic::~CGUIHTMLResizepic()
{
}

void CGUIHTMLResizepic::Scroll(bool up, int delay)
{
    DEBUG_TRACE_FUNCTION;
    if (m_HTMLGump != nullptr)
        m_HTMLGump->Scroll(up, delay);
}


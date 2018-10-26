// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** GUIHTMLButton.cpp
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#include "stdafx.h"

CGUIHTMLButton::CGUIHTMLButton(
    CGUIHTMLGump *htmlGump,
    int serial,
    ushort graphic,
    ushort graphicSelected,
    ushort graphicPressed,
    int x,
    int y)
    : CGUIButton(serial, graphic, graphicSelected, graphicPressed, x, y)
    , m_HTMLGump(htmlGump)
{
}

CGUIHTMLButton::~CGUIHTMLButton()
{
}

void CGUIHTMLButton::SetShaderMode()
{
    DEBUG_TRACE_FUNCTION;
    glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
}

void CGUIHTMLButton::Scroll(bool up, int delay)
{
    DEBUG_TRACE_FUNCTION;
    if (m_HTMLGump != nullptr)
        m_HTMLGump->Scroll(up, delay);
}


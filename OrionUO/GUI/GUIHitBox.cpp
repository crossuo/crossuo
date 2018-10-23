// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** GUIHitBox.cpp
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#include "stdafx.h"

CGUIHitBox::CGUIHitBox(int serial, int x, int y, int width, int height, bool callOnMouseUp)
    : CGUIPolygonal(GOT_HITBOX, x, y, width, height, callOnMouseUp)
{
    Serial = serial;
}

CGUIHitBox::~CGUIHitBox()
{
}

void CGUIHitBox::OnMouseEnter()
{
    DEBUG_TRACE_FUNCTION;
    if (g_SelectedObject.Gump != NULL)
        g_SelectedObject.Gump->WantRedraw = true;
}

void CGUIHitBox::OnMouseExit()
{
    DEBUG_TRACE_FUNCTION;
    if (g_LastSelectedObject.Gump != NULL)
        g_LastSelectedObject.Gump->WantRedraw = true;
}


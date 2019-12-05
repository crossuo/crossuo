// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIHitBox.h"
#include "../SelectedObject.h"
#include "../Gumps/Gump.h"

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
    if (g_SelectedObject.Gump != nullptr)
    {
        g_SelectedObject.Gump->WantRedraw = true;
    }
}

void CGUIHitBox::OnMouseExit()
{
    if (g_LastSelectedObject.Gump != nullptr)
    {
        g_LastSelectedObject.Gump->WantRedraw = true;
    }
}

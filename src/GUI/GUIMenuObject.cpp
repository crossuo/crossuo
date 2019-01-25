// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIMenuObject.h"
#include "../CrossUO.h"
#include "../Point.h"
#include "../SelectedObject.h"
#include "../Managers/MouseManager.h"
#include "../Gumps/Gump.h"
#include "../Gumps/GumpMenu.h"

CGUIMenuObject::CGUIMenuObject(
    int serial, uint16_t graphic, uint16_t color, int x, int y, const string &text)
    : CGUITilepic(graphic, color, x, y)
    , Text(text)
{
    Serial = serial;
    MoveOnDrag = true;
}

CGUIMenuObject::~CGUIMenuObject()
{
}

bool CGUIMenuObject::Select()
{
    DEBUG_TRACE_FUNCTION;
    int x = g_MouseManager.Position.X - m_X;
    int y = g_MouseManager.Position.Y - m_Y;

    CSize size = g_Game.GetStaticArtDimension(Graphic);

    return (x >= 0 && y >= 0 && x < size.Width && y < size.Height);
}

void CGUIMenuObject::OnMouseEnter()
{
    DEBUG_TRACE_FUNCTION;
    if (g_SelectedObject.Gump != nullptr && g_SelectedObject.Gump->GumpType == GT_MENU)
    {
        CGumpMenu *menu = (CGumpMenu *)g_SelectedObject.Gump;

        if (menu->Text != Text)
        {
            menu->Text = Text;
            menu->TextChanged = true;
        }
    }
}

void CGUIMenuObject::OnMouseExit()
{
    DEBUG_TRACE_FUNCTION;
    if (g_LastSelectedObject.Gump != nullptr && g_LastSelectedObject.Gump->GumpType == GT_MENU)
    {
        CGumpMenu *menu = (CGumpMenu *)g_LastSelectedObject.Gump;

        if (menu->Text == Text)
        {
            menu->Text = "";
            menu->TextChanged = true;
        }
    }
}

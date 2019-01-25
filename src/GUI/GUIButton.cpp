// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIButton.h"
#include "../CrossUO.h"
#include "../SelectedObject.h"
#include "../Gumps/Gump.h"

CGUIButton::CGUIButton(
    int serial, uint16_t graphic, uint16_t graphicSelected, uint16_t graphicPressed, int x, int y)
    : CGUIDrawObject(GOT_BUTTON, serial, graphic, 0, x, y)
    , GraphicSelected(graphicSelected)
    , GraphicPressed(graphicPressed)
{
}

CGUIButton::~CGUIButton()
{
}

void CGUIButton::PrepareTextures()
{
    DEBUG_TRACE_FUNCTION;
    g_Game.ExecuteGump(Graphic);
    g_Game.ExecuteGump(GraphicSelected);
    g_Game.ExecuteGump(GraphicPressed);
}

uint16_t CGUIButton::GetDrawGraphic()
{
    DEBUG_TRACE_FUNCTION;
    uint16_t graphic = Graphic;

    if (g_GumpPressedElement == this)
    {
        graphic = GraphicPressed;
    }
    else if (g_GumpSelectedElement == this)
    {
        graphic = GraphicSelected;
    }

    return graphic;
}

void CGUIButton::OnMouseEnter()
{
    DEBUG_TRACE_FUNCTION;
    if (g_SelectedObject.Gump != nullptr)
    {
        g_SelectedObject.Gump->WantRedraw = true;
    }
}

void CGUIButton::OnMouseExit()
{
    DEBUG_TRACE_FUNCTION;
    if (g_LastSelectedObject.Gump != nullptr)
    {
        g_LastSelectedObject.Gump->WantRedraw = true;
    }
}

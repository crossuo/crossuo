// MIT License
// Copyright (C) May 2017 Hotride

#include "GUIVirtueGump.h"
#include "../CrossUO.h"
#include "../Managers/MouseManager.h"

CGUIVirtueGump::CGUIVirtueGump(uint16_t graphic, int x, int y)
    : CGUIDrawObject(GOT_VIRTUE_GUMP, graphic, graphic, 0, x, y)
{
}

CGUIVirtueGump::~CGUIVirtueGump()
{
}

bool CGUIVirtueGump::Select()
{
    bool select = false;
    auto spr = g_Game.ExecuteGump(Graphic);
    if (spr != nullptr)
    {
        const int x = g_MouseManager.Position.X - m_X;
        const int y = g_MouseManager.Position.Y - m_Y;
        select = (x >= 0 && y >= 0 && x < spr->Width && y < spr->Height);
    }
    return select;
}

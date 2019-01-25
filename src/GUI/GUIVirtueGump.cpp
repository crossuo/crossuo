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

    CGLTexture *th = g_Game.ExecuteGump(Graphic);

    if (th != nullptr)
    {
        int x = g_MouseManager.Position.X - m_X;
        int y = g_MouseManager.Position.Y - m_Y;

        select = (x >= 0 && y >= 0 && x < th->Width && y < th->Height);
    }

    return select;
}

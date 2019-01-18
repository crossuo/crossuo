// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIGumppicTiled.h"
#include "../OrionUO.h"
#include "../Managers/MouseManager.h"

CGUIGumppicTiled::CGUIGumppicTiled(uint16_t graphic, int x, int y, int width, int height)
    : CGUIDrawObject(GOT_GUMPPICTILED, 0, graphic, 0, x, y)
    , Width(width)
    , Height(height)
{
}

CGUIGumppicTiled::~CGUIGumppicTiled()
{
}

void CGUIGumppicTiled::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    CGLTexture *th = g_Orion.ExecuteGump(Graphic);

    if (th != nullptr)
    {
        SetShaderMode();

        th->Draw(m_X, m_Y, Width, Height, checktrans);
    }
}

bool CGUIGumppicTiled::Select()
{
    DEBUG_TRACE_FUNCTION;
    int x = g_MouseManager.Position.X - m_X;
    int y = g_MouseManager.Position.Y - m_Y;

    if (x < 0 || y < 0 || (Width > 0 && x >= Width) || (Height > 0 && y >= Height))
    {
        return false;
    }

    CGLTexture *th = g_Orion.ExecuteGump(Graphic);

    if (th != nullptr)
    {
        int width = Width;
        int height = Height;

        if (width == 0)
        {
            width = th->Width;
        }

        if (height == 0)
        {
            height = th->Height;
        }

        while (x > th->Width && width > th->Width)
        {
            x -= th->Width;
            width -= th->Width;
        }

        while (y > th->Height && height > th->Height)
        {
            y -= th->Height;
            height -= th->Height;
        }

        if (x > width || y > height)
        {
            return false;
        }

        if (x >= 0 && y >= 0 && x < th->Width && y < th->Height)
        {
            if (CheckPolygone)
            {
                return true;
            }

            int pos = (y * th->Width) + x;

            if (pos < (int)th->m_HitMap.size())
            {
                return (th->m_HitMap[pos] != 0);
            }
        }
    }

    return false;
}

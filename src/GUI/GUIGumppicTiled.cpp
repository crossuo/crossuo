// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIGumppicTiled.h"
#include "../CrossUO.h"
#include "../Sprite.h"
#include "../Managers/MouseManager.h"
#include "../Utility/PerfMarker.h"

CGUIGumppicTiled::CGUIGumppicTiled(uint16_t graphic, int x, int y, int width, int height)
    : CGUIDrawObject(GOT_GUMPPICTILED, 0, graphic, 0, x, y)
    , Width(width)
    , Height(height)
{
}

void CGUIGumppicTiled::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);

    auto spr = g_Game.ExecuteGump(Graphic);
    if (spr != nullptr && spr->Texture != nullptr)
    {
        SetShaderMode();
        spr->Texture->Draw(m_X, m_Y, Width, Height, checktrans);
    }
}

bool CGUIGumppicTiled::Select()
{
    int x = g_MouseManager.Position.X - m_X;
    int y = g_MouseManager.Position.Y - m_Y;
    if (x < 0 || y < 0 || (Width > 0 && x >= Width) || (Height > 0 && y >= Height))
    {
        return false;
    }

    auto spr = g_Game.ExecuteGump(Graphic);
    if (spr != nullptr)
    {
        int width = Width;
        int height = Height;
        if (width == 0)
        {
            width = spr->Width;
        }
        if (height == 0)
        {
            height = spr->Height;
        }

        while (x > spr->Width && width > spr->Width)
        {
            x -= spr->Width;
            width -= spr->Width;
        }

        while (y > spr->Height && height > spr->Height)
        {
            y -= spr->Height;
            height -= spr->Height;
        }

        if (x > width || y > height)
        {
            return false;
        }
        return spr->TestHit(x, y, !BoundingBoxCheck);
    }
    return false;
}

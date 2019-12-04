// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIScrollBackground.h"
#include "../CrossUO.h"
#include "../Sprite.h"
#include "../Managers/MouseManager.h"
#include "../Utility/PerfMarker.h"

CGUIScrollBackground::CGUIScrollBackground(int serial, uint16_t graphic, int x, int y, int height)
    : CBaseGUI(GOT_SCROLLBACKGROUND, serial, graphic, 0, x, y)
    , Height(height)
{
    DEBUG_TRACE_FUNCTION;
    OffsetX = 0;
    BottomOffsetX = 0;
    Width = 0;
    int width = 0;

    CSprite *spr[4] = { nullptr };
    bool isValid = true;
    for (int i = 0; i < 4; i++)
    {
        spr[i] = g_Game.ExecuteGump(Graphic + (int)i);
        if (spr[i] != nullptr)
        {
            if (width < spr[i]->Width)
            {
                width = spr[i]->Width;
            }
        }
        else
        {
            isValid = false;
        }
    }

    if (isValid)
    {
        OffsetX = (width - spr[1]->Width) / 2;
        int offset = spr[0]->Width - spr[3]->Width;
        BottomOffsetX = (offset / 2) + (offset / 4);
        Width = width; // m_OffsetX + spr[3]->Width;
    }

    UpdateHeight(Height);
}

void CGUIScrollBackground::UpdateHeight(int height)
{
    DEBUG_TRACE_FUNCTION;
    Height = height;

    CSprite *spr[4] = { nullptr };
    for (int i = 0; i < 4; i++)
    {
        spr[i] = g_Game.ExecuteGump(Graphic + (int)i);
        if (spr[i] == nullptr)
        {
            return;
        }
    }

    const auto h = spr[0]->Height;
    WorkSpace = CRect(OffsetX + 10, h, spr[1]->Width - 20, Height - (h + spr[3]->Height));
}

void CGUIScrollBackground::PrepareTextures()
{
    DEBUG_TRACE_FUNCTION;
    g_Game.ExecuteGumpPart(Graphic, 4);
}

void CGUIScrollBackground::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
    DEBUG_TRACE_FUNCTION;
    CGLTexture *th[4] = { nullptr };

    for (int i = 0; i < 4; i++)
    {
        auto spr = g_Game.ExecuteGump(Graphic + (int)i);
        if (spr == nullptr)
        {
            return;
        }
        assert(spr->Texture != nullptr);
        th[i] = spr->Texture;
    }

    th[0]->Draw(m_X, m_Y, checktrans); //Top scroll

    int currentY = th[0]->Height;
    int height = Height - th[3]->Height;

    int bodyX = m_X + OffsetX;

    bool exit = false;

    while (!exit)
    {
        for (int i = 1; i < 3; i++)
        {
            int deltaHeight = height - currentY;

            if (deltaHeight < th[i]->Height)
            {
                if (deltaHeight > 0)
                {
                    th[i]->Draw(bodyX, m_Y + currentY, 0, deltaHeight, checktrans);
                }

                exit = true;
                break;
            }
            {
                th[i]->Draw(bodyX, m_Y + currentY, checktrans);
            }

            currentY += th[i]->Height;
        }
    }

    th[3]->Draw(m_X + BottomOffsetX, m_Y + Height - th[3]->Height, checktrans); //Bottom scroll
}

bool CGUIScrollBackground::Select()
{
    DEBUG_TRACE_FUNCTION;
    int x = g_MouseManager.Position.X - m_X;
    int y = g_MouseManager.Position.Y - m_Y;

    if (!(x >= 0 && y >= 0 && x < Width && y < Height))
    {
        return false;
    }

    if (BoundingBoxCheck)
    {
        return true;
    }

    bool select = false;
    CSprite *spr[4] = { nullptr };
    for (int i = 0; i < 4; i++)
    {
        spr[i] = g_Game.ExecuteGump(Graphic + (int)i);
        if (spr[i] == nullptr)
        {
            return false;
        }
    }

    y = m_Y;
    select =
        g_Game.GumpPixelsInXY(Graphic, m_X, y) ||
        g_Game.GumpPixelsInXY(
            Graphic + 3, m_X + BottomOffsetX, y + Height - spr[3]->Height); //Top/Bottom scrolls

    x = m_X + OffsetX;
    int currentY = spr[0]->Height;
    int height = Height - spr[3]->Height;
    while (!select)
    {
        for (int i = 1; i < 3 && !select; i++)
        {
            int deltaHeight = height - currentY;
            if (deltaHeight < spr[i]->Height)
            {
                if (deltaHeight > 0)
                {
                    select =
                        g_Game.GumpPixelsInXY(Graphic + (int)i, x, y + currentY, 0, deltaHeight);
                }
                return select;
            }
            {
                select = g_Game.GumpPixelsInXY(Graphic + (int)i, x, y + currentY);
            }
            currentY += spr[i]->Height;
        }
    }
    return select;
}

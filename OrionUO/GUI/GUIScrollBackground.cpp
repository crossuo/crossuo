// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIScrollBackground.h"
#include "../OrionUO.h"
#include "../Managers/MouseManager.h"

CGUIScrollBackground::CGUIScrollBackground(int serial, uint16_t graphic, int x, int y, int height)
    : CBaseGUI(GOT_SCROLLBACKGROUND, serial, graphic, 0, x, y)
    , Height(height)
{
    DEBUG_TRACE_FUNCTION;
    OffsetX = 0;
    BottomOffsetX = 0;
    Width = 0;
    int width = 0;

    CGLTexture *th[4] = { nullptr };
    bool isValid = true;

    for (int i = 0; i < 4; i++)
    {
        th[i] = g_Orion.ExecuteGump(Graphic + (int)i);

        if (th[i] != nullptr)
        {
            if (width < th[i]->Width)
            {
                width = th[i]->Width;
            }
        }
        else
        {
            isValid = false;
        }
    }

    if (isValid)
    {
        OffsetX = (width - th[1]->Width) / 2;
        int offset = th[0]->Width - th[3]->Width;
        BottomOffsetX = (offset / 2) + (offset / 4);
        Width = width; // m_OffsetX + th[3]->Width;
    }

    UpdateHeight(Height);
}

CGUIScrollBackground::~CGUIScrollBackground()
{
}

void CGUIScrollBackground::UpdateHeight(int height)
{
    DEBUG_TRACE_FUNCTION;
    Height = height;

    CGLTexture *th[4] = { nullptr };

    for (int i = 0; i < 4; i++)
    {
        th[i] = g_Orion.ExecuteGump(Graphic + (int)i);

        if (th[i] == nullptr)
        {
            return;
        }
    }

    WorkSpace = CRect(
        OffsetX + 10, th[0]->Height, th[1]->Width - 20, Height - (th[0]->Height + th[3]->Height));
}

void CGUIScrollBackground::PrepareTextures()
{
    DEBUG_TRACE_FUNCTION;
    g_Orion.ExecuteGumpPart(Graphic, 4);
}

void CGUIScrollBackground::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    CGLTexture *th[4] = { nullptr };

    for (int i = 0; i < 4; i++)
    {
        th[i] = g_Orion.ExecuteGump(Graphic + (int)i);

        if (th[i] == nullptr)
        {
            return;
        }
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

    if (CheckPolygone)
    {
        return true;
    }

    bool select = false;

    CGLTexture *th[4] = { nullptr };

    for (int i = 0; i < 4; i++)
    {
        th[i] = g_Orion.ExecuteGump(Graphic + (int)i);

        if (th[i] == nullptr)
        {
            return false;
        }
    }

    y = m_Y;

    select = g_Orion.GumpPixelsInXY(Graphic, m_X, y) ||
             g_Orion.GumpPixelsInXY(
                 Graphic + 3, m_X + BottomOffsetX, y + Height - th[3]->Height); //Top/Bottom scrolls

    x = m_X + OffsetX;

    int currentY = th[0]->Height;
    int height = Height - th[3]->Height;

    while (!select)
    {
        for (int i = 1; i < 3 && !select; i++)
        {
            int deltaHeight = height - currentY;

            if (deltaHeight < th[i]->Height)
            {
                if (deltaHeight > 0)
                {
                    select =
                        g_Orion.GumpPixelsInXY(Graphic + (int)i, x, y + currentY, 0, deltaHeight);
                }

                return select;
            }
            {
                select = g_Orion.GumpPixelsInXY(Graphic + (int)i, x, y + currentY);
            }

            currentY += th[i]->Height;
        }
    }

    return select;
}

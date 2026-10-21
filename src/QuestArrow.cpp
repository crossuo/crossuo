// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "QuestArrow.h"
#include "Globals.h"
#include "Renderer/RenderShader.h" // g_ColorizerShader and friends
#include "CrossUO.h"
#include "Point.h"
#include "Managers/MouseManager.h"
#include "ScreenStages/GameScreen.h"
#include "GameObjects/GamePlayer.h"
#include "Utility/PerfMarker.h"
#include <cstdint>

CQuestArrow g_QuestArrow;

CQuestArrow::CQuestArrow()
= default;

CQuestArrow::~CQuestArrow()
= default;

void CQuestArrow::Draw()
{
    ScopedPerfMarker(__FUNCTION__);

    m_ScreenX = -1;
    m_ScreenY = -1;
    if (Enabled)
    {
        int dir = g_MouseManager.GetFacing(g_Player->GetX(), g_Player->GetY(), X, Y, 0);

        uint16_t gumpID = m_Gump + ((dir + 1) % 8);

        CSize size = g_Game.GetGumpDimension(gumpID);
        m_Width = size.Width;
        m_Height = size.Height;

        int gox = X - g_Player->GetX();
        int goy = Y - g_Player->GetY();

        int x = g_RenderBounds.GameWindowCenterX + ((gox - goy) * 22) - (size.Width / 2);
        int y = g_RenderBounds.GameWindowCenterY + ((gox + goy) * 22) - (size.Height / 2);

        if (x < g_RenderBounds.GameWindowPosX)
        {
            x = g_RenderBounds.GameWindowPosX;
        }
        else if (x > g_RenderBounds.GameWindowPosX + g_RenderBounds.GameWindowWidth - size.Width)
        {
            x = g_RenderBounds.GameWindowPosX + g_RenderBounds.GameWindowWidth - size.Width;
        }

        if (y < g_RenderBounds.GameWindowPosY)
        {
            y = g_RenderBounds.GameWindowPosY;
        }
        else if (y > g_RenderBounds.GameWindowPosY + g_RenderBounds.GameWindowHeight - size.Height)
        {
            y = g_RenderBounds.GameWindowPosY + g_RenderBounds.GameWindowHeight - size.Height;
        }

        // remember the clamped screen rect so the arrow can be clicked
        m_ScreenX = x;
        m_ScreenY = y;

        if (Timer < g_Ticks)
        {
            if (Timer + 120 < g_Ticks)
            {
                Timer = g_Ticks + 1000;
            }

            g_ColorizerShader.Enable();
            g_Game.DrawGump(gumpID, 0x0021, x, y);
            g_ColorizerShader.Disable();
        }
        else
        {
            g_Game.DrawGump(gumpID, 0, x, y);
        }
    }
}

bool CQuestArrow::Contains(int x, int y) const
{
    if (!Enabled || m_ScreenX == -1)
    {
        return false;
    }
    return (
        x >= m_ScreenX && x < m_ScreenX + m_Width && y >= m_ScreenY && y < m_ScreenY + m_Height);
}

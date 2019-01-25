// MIT License
// Copyright (C) August 2016 Hotride

#include "TargetGump.h"
#include "CrossUO.h"
#include "Managers/ConfigManager.h"

CTargetGump g_TargetGump;
CTargetGump g_AttackTargetGump;
CNewTargetSystem g_NewTargetSystem;

CTargetGump::CTargetGump()
{
}

CTargetGump::~CTargetGump()
{
}

void CTargetGump::Draw()
{
    DEBUG_TRACE_FUNCTION;
    if (Color != 0)
    {
        g_Game.DrawGump(0x1068, Color, X, Y);

        if (Hits > 0)
        {
            g_Game.DrawGump(0x1069, HealthColor, X, Y, Hits, 0);
        }
    }
}

CNewTargetSystem::CNewTargetSystem()
{
}

CNewTargetSystem::~CNewTargetSystem()
{
}

void CNewTargetSystem::Draw()
{
    DEBUG_TRACE_FUNCTION;
    if (!g_ConfigManager.DisableNewTargetSystem && ColorGump != 0)
    {
        CIndexObject &top = g_Game.m_GumpDataIndex[GumpTop];

        int x = X - (top.Width / 2);

        g_Game.DrawGump(GumpTop, 0, x, TopY - top.Height);
        g_Game.DrawGump(ColorGump, 0, x, TopY - top.Height);
        g_Game.DrawGump(GumpBottom, 0, x, BottomY);

        if (Hits > 0)
        {
            g_Game.DrawGump(0x1069, HealthColor, X - 16, BottomY + 15, Hits, 0);
        }
    }
}

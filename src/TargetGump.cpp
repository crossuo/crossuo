// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "TargetGump.h"
#include "CrossUO.h"
#include <xuocore/uodata.h>
#include "Managers/ConfigManager.h"
#include "Utility/PerfMarker.h"

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
    ScopedPerfMarker(__FUNCTION__);

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
    ScopedPerfMarker(__FUNCTION__);

    if (!g_ConfigManager.DisableNewTargetSystem && ColorGump != 0)
    {
        CIndexObject &top = g_Index.m_Gump[GumpTop];

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

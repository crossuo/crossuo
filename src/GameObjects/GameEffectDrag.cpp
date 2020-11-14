// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GameEffectDrag.h"
#include "../Managers/EffectManager.h"
#include "../GameObjects/GameEffectMoving.h"
#include "Globals.h"

CGameEffectDrag::CGameEffectDrag()
{
}

CGameEffectDrag::~CGameEffectDrag()
{
}

void CGameEffectDrag::Update(CGameObject *parent)
{
    if (LastMoveTime > g_Ticks)
    {
        return;
    }

    OffsetX += 8;
    OffsetY += 8;

    LastMoveTime = g_Ticks + MoveDelay;

    if (Duration < g_Ticks)
    {
        g_EffectManager.RemoveEffect(this);
    }
}

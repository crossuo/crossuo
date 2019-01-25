// MIT License
// Copyright (C) December 2016 Hotride

#include "GumpRacialAbility.h"
#include "../ToolTip.h"
#include "../Managers/ClilocManager.h"
#include "../Network/Packets.h"
#include "../GameObjects/GamePlayer.h"

CGumpRacialAbility::CGumpRacialAbility(int serial, int x, int y)
    : CGump(GT_RACIAL_ABILITY, serial, x, y)
{
    DEBUG_TRACE_FUNCTION;
    m_Locker.Serial = ID_GS_LOCK_MOVING;

    Add(new CGUIGumppic(serial, 0, 0));
}

CGumpRacialAbility::~CGumpRacialAbility()
{
}

void CGumpRacialAbility::InitToolTip()
{
    DEBUG_TRACE_FUNCTION;
    g_ToolTip.Set(g_ClilocManager.Cliloc(g_Language)->GetW(1112198 + (Serial - 0x5DD0), true), 200);
}

void CGumpRacialAbility::GUMP_BUTTON_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    if (serial == ID_GS_LOCK_MOVING)
    {
        LockMoving = !LockMoving;
    }
}

void CGumpRacialAbility::OnAbilityUse(uint16_t index)
{
    DEBUG_TRACE_FUNCTION;
    switch (index)
    {
        case 0x5DDA:
        {
            if (g_Player->Race == RT_GARGOYLE)
            {
                CPacketToggleGargoyleFlying().Send();
            }

            break;
        }
        default:
            break;
    }
}

bool CGumpRacialAbility::OnLeftMouseButtonDoubleClick()
{
    DEBUG_TRACE_FUNCTION;
    OnAbilityUse((uint16_t)Serial);

    return true;
}

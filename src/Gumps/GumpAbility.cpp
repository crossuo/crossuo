// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GumpAbility.h"
#include "../ToolTip.h"
#include "../Managers/ClilocManager.h"
#include "../Managers/GumpManager.h"
#include "../Network/Packets.h"
#include "../Globals.h" // g_Ability

CGumpAbility::CGumpAbility(int serial, int x, int y)
    : CGump(GT_ABILITY, serial, x, y)
{
    int index = (Serial != 0 ? 1 : 0);
    uint8_t &ability = g_Ability[index];

    Graphic = 0x5200 + (ability & 0x7F) - 1;

    m_Locker.Serial = ID_GS_LOCK_MOVING;

    m_Colorizer =
        (CGUIGlobalColor *)Add(new CGUIGlobalColor((ability & 0x80) != 0, 1.0f, 0.25f, 0.5f, 1.0f));

    m_Body = (CGUIGumppic *)Add(new CGUIGumppic(Graphic, 0, 0));
}

CGumpAbility::~CGumpAbility()
{
}

void CGumpAbility::InitToolTip()
{
    int index = (Serial != 0 ? 1 : 0);

    g_ToolTip.Set(
        g_ClilocManager.Cliloc(g_Language)->GetW(1028838 + (g_Ability[index] & 0x7F) - 1, true),
        80);
}

void CGumpAbility::UpdateContent()
{
    if (m_Colorizer != nullptr && m_Body != nullptr)
    {
        int index = (Serial != 0 ? 1 : 0);
        uint8_t &ability = g_Ability[index];

        m_Colorizer->Enabled = ((ability & 0x80) != 0);

        m_Body->Graphic = 0x5200 + (ability & 0x7F) - 1;
    }
}

void CGumpAbility::GUMP_BUTTON_EVENT_C
{
    if (serial == ID_GS_LOCK_MOVING)
    {
        LockMoving = !LockMoving;
    }
}

void CGumpAbility::OnAbilityUse(int index)
{
    uint8_t &ability = g_Ability[index];

    if ((ability & 0x80) == 0)
    {
        for (int i = 0; i < 2; i++)
        {
            g_Ability[i] &= 0x7F;
        }

        CPacketUseCombatAbility(ability).Send();
    }
    else
    {
        CPacketUseCombatAbility(0).Send();
    }

    ability ^= 0x80;

    g_GumpManager.UpdateContent(0, 0, GT_ABILITY);
    g_GumpManager.UpdateContent(1, 0, GT_ABILITY);
}

bool CGumpAbility::OnLeftMouseButtonDoubleClick()
{
    OnAbilityUse(Serial != 0 ? 1 : 0);
    return true;
}

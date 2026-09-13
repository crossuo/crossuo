// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/corpsemanager.cpp). The timer-based
// pieces moved from elapsed timers to g_Ticks stamps (plan §4.2).

#include "corpsemanager.h"
#include "../core/assist.h"
#include "../config/assistantconfig.h"
#include "../model/gameobjects/gameworld.h"
#include "../model/gameobjects/gameplayer.h"

namespace xuoassist {

CCorpseManager g_CorpseManager;

CCorpseManager::CCorpseManager() {}

CCorpseManager::~CCorpseManager() {}

void CCorpseManager::Add(const CCorpseInfo &info)
{
    for (CORPSE_INFO_LIST::iterator i = m_List.begin(); i != m_List.end(); ++i)
    {
        if (i->GetSerial() == info.GetSerial())
        {
            i->SetTimer(info.GetTimer());
            return;
        }
    }

    m_List.push_back(info);

    // Was g_AssistantForm->OnNewCorpse() (started the 10 ms corpse timer);
    // CheckCorpses() now runs from the assistant frame tick (Phase 3 wires
    // the tick, plan D3).
}

void CCorpseManager::Clear()
{
    m_List.clear();
}

void CCorpseManager::CheckCorpses()
{
    if (g_World == nullptr || g_Player == nullptr)
    {
        return;
    }

    int useTicks = (int)(g_Ticks - g_LastUseObjectTimer);

    if (useTicks < g_AssistantConfig.UseItemsDelay())
    {
        return;
    }

    int globalTicks = (int)g_Ticks; // was g_StartTimer.elapsed()
    int useDistance = g_AssistantConfig.OpenCorpsesDistance();

    for (CORPSE_INFO_LIST::iterator i = m_List.begin(); i != m_List.end();)
    {
        CGameItem *item = g_World->FindWorldItem(i->GetSerial());

        if (item == nullptr)
        {
            ++i;
            continue;
        }
        if (item->GetContainer() != 0xFFFFFFFF)
        {
            i = m_List.erase(i);
            continue;
        }

        if (GetDistance(g_Player, item) <= useDistance)
        {
            g_Assistant.DoubleClick(item->GetSerial());
            g_LastUseObjectTimer = g_Ticks; // was g_LastUseObjectTimer.restart()
            m_List.erase(i);
            return;
        }

        if (i->GetTimer() < globalTicks)
        {
            i = m_List.erase(i);
        }
        else
        {
            ++i;
        }
    }
}

} // namespace xuoassist

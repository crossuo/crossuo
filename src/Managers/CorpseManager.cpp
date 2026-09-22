// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2017 Hotride

#include "CorpseManager.h"
#include "../CrossUO.h"
#include "../Globals.h"
#include "../GameObjects/GameItem.h"
#include "../GameObjects/GamePlayer.h"
#include "../GameObjects/GameWorld.h"
#if defined(XUO_ASSISTANT)
#include <core/assist.h> // xuoassist::g_Assistant (DoubleClick wraps the recording suspension)
#endif

CCorpseManager g_CorpseManager;

void CCorpseManager::Add(const CCorpse &corpse)
{
    for (const CCorpse &item : m_List)
    {
        if (item.CorpseSerial == corpse.CorpseSerial)
        {
            return;
        }
    }

    m_List.push_back(corpse);
}

void CCorpseManager::Remove(int corpseSerial, int objectSerial)
{
    for (auto i = m_List.begin(); i != m_List.end();)
    {
        if (i->CorpseSerial == corpseSerial || i->ObjectSerial == objectSerial)
        {
            if (corpseSerial != 0)
            {
                CGameItem *obj = g_World->FindWorldItem(corpseSerial);

                if (obj != nullptr)
                {
                    obj->Layer = (i->Direction & 7) | (i->Running ? 0x80 : 0);
                }
            }

            i = m_List.erase(i);
        }
        else
        {
            ++i;
        }
    }
}

bool CCorpseManager::InList(int corpseSerial, int objectSerial)
{
    for (const CCorpse &item : m_List)
    {
        if (item.CorpseSerial == corpseSerial || item.ObjectSerial == objectSerial)
        {
            return true;
        }
    }

    return false;
}

CGameObject *CCorpseManager::GetCorpseObject(int serial)
{
    for (const CCorpse &item : m_List)
    {
        if (item.ObjectSerial == serial)
        {
            return g_World->FindWorldObject(item.CorpseSerial);
        }
    }

    return nullptr;
}

void CCorpseManager::AddAutoOpen(int serial, int timer)
{
    for (CCorpseToOpen &item : m_AutoOpenList)
    {
        if (item.Serial == (uint32_t)serial)
        {
            item.Timer = (uint32_t)timer;
            return;
        }
    }

    m_AutoOpenList.push_back(CCorpseToOpen(serial, timer));
}

bool CCorpseManager::CheckCorpses(int useDistance, int useDelay, uint32_t lastUseTime)
{
    if (g_World == nullptr || g_Player == nullptr)
    {
        return false;
    }

    if ((int)(g_Ticks - lastUseTime) < useDelay)
    {
        return false;
    }

    for (auto i = m_AutoOpenList.begin(); i != m_AutoOpenList.end();)
    {
        CGameItem *item = g_World->FindWorldItem(i->Serial);

        if (item == nullptr)
        {
            ++i;
            continue;
        }

        if (item->Container != 0xFFFFFFFF)
        {
            i = m_AutoOpenList.erase(i);
            continue;
        }

        if (GetDistance(g_Player, item) <= useDistance)
        {
#if defined(XUO_ASSISTANT)
            // the assistant's own DoubleClick wraps the recording suspension:
            // auto-opened corpses do not record into user macros
            xuoassist::g_Assistant.DoubleClick(item->Serial);
#else
            g_Game.DoubleClick(item->Serial);
#endif
            m_AutoOpenList.erase(i);
            return true;
        }

        if (i->Timer < g_Ticks)
        {
            i = m_AutoOpenList.erase(i);
        }
        else
        {
            ++i;
        }
    }

    return false;
}

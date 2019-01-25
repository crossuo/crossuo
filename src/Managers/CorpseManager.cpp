// MIT License
// Copyright (C) August 2017 Hotride

#include "CorpseManager.h"
#include "../GameObjects/GameItem.h"
#include "../GameObjects/GameWorld.h"

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
    for (deque<CCorpse>::iterator i = m_List.begin(); i != m_List.end();)
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

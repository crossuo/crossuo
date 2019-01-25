// MIT License
// Copyright (C) September 2017 Hotride

#include "CrossUO.h"
#include "UseItemsList.h"
#include "GameObjects/GameWorld.h"

CUseItemActions g_UseItemActions;

void CUseItemActions::Add(int serial)
{
    DEBUG_TRACE_FUNCTION;
    for (deque<uint32_t>::iterator i = m_List.begin(); i != m_List.end(); ++i)
    {
        if (*i == serial)
        {
            return;
        }
    }

    m_List.push_back(serial);
}

void CUseItemActions::Process()
{
    DEBUG_TRACE_FUNCTION;
    if (Timer <= g_Ticks)
    {
        Timer = g_Ticks + 1000;

        if (m_List.empty())
        {
            return;
        }

        uint32_t serial = m_List.front();
        m_List.pop_front();

        if (g_World->FindWorldObject(serial) != nullptr)
        {
            if (serial < 0x40000000)
            { //NPC
                g_Game.PaperdollReq(serial);
            }
            else
            { //item
                g_Game.DoubleClick(serial);
            }
        }
    }
}

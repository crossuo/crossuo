// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/trademanager.cpp).

#include "trademanager.h"
#include "../model/gameobjects/gameworld.h"
#include "../core/assist.h"
#include <common/logging/logging.h>

namespace xuoassist {

static string to_lower_copy(string text)
{
    for (char &c : text)
    {
        c = (char)tolower(c);
    }
    return text;
}

CTradeManager g_TradeManager;

CTradeManager::CTradeManager() {}

CTradeManager::~CTradeManager()
{
    m_List.clear();
}

bool CTradeManager::OnPacketSend(CDataReader &reader)
{
    if (g_World == nullptr)
    {
        return true;
    }

    int action = reader.ReadUInt8();
    uint id = reader.ReadUInt32BE();

    switch ((TRADE_ACTIONS)action)
    {
        case TA_CLOSE:
        {
            for (size_t i = 0; i < m_List.size(); i++)
            {
                CTradeWindow &trade = m_List[i];

                if (trade.GetID1() == id)
                {
                    CGameObject *obj = g_World->FindWorldObject(trade.GetID1());

                    if (obj != nullptr)
                    {
                        g_World->RemoveObject(obj);
                    }

                    obj = g_World->FindWorldObject(trade.GetID2());

                    if (obj != nullptr)
                    {
                        g_World->RemoveObject(obj);
                    }

                    m_List.erase(m_List.begin() + i);

                    return true;
                }
            }

            Warning(Assistant, "Warning!!! Unknown trade closed\n");

            break;
        }
        case TA_CHECK:
        {
            for (CTradeWindow &trade : m_List)
            {
                if (trade.GetID1() == id)
                {
                    trade.SetCheck1((bool)reader.ReadUInt8());
                    return true;
                }
            }

            Warning(Assistant, "Warning!!! Unknown trade changed\n");

            break;
        }
        default:
        {
            Warning(Assistant, "Warning!!! Unknown trade action\n");
            break;
        }
    }

    return true;
}

bool CTradeManager::OnPacketReceived(CDataReader &reader)
{
    if (g_World == nullptr)
    {
        return true;
    }

    int action = reader.ReadUInt8();
    uint serial = reader.ReadUInt32BE();
    uint id1 = reader.ReadUInt32BE();
    uint id2 = reader.ReadUInt32BE();

    switch ((TRADE_ACTIONS)action)
    {
        case TA_OPEN:
        {
            string name = "";

            if (reader.ReadUInt8() != 0)
            {
                name = reader.ReadString();
            }

            m_List.push_back(CTradeWindow(serial, id1, id2, name));

            break;
        }
        case TA_CLOSE:
        {
            for (size_t i = 0; i < m_List.size(); i++)
            {
                CTradeWindow &trade = m_List[i];

                if (trade.GetID1() == serial)
                {
                    CGameObject *obj = g_World->FindWorldObject(trade.GetID1());

                    if (obj != nullptr)
                    {
                        g_World->RemoveObject(obj);
                    }

                    obj = g_World->FindWorldObject(trade.GetID2());

                    if (obj != nullptr)
                    {
                        g_World->RemoveObject(obj);
                    }

                    m_List.erase(m_List.begin() + i);

                    return true;
                }
            }

            Warning(Assistant, "Warning!!! Unknown trade closed\n");

            break;
        }
        case TA_CHECK:
        {
            for (CTradeWindow &trade : m_List)
            {
                if (trade.GetID1() == serial)
                {
                    trade.SetCheck1((bool)id1);
                    trade.SetCheck2((bool)id2);
                    return true;
                }
            }

            Warning(Assistant, "Warning!!! Unknown trade changed\n");

            break;
        }
        default:
        {
            Warning(Assistant, "Warning!!! Unknown trade action\n");
            break;
        }
    }

    return true;
}

void CTradeManager::Clear()
{
    m_List.clear();
}

int CTradeManager::Count()
{
    return (int)m_List.size();
}

CTradeWindow *CTradeManager::GetTrade(const string &serial)
{
    uint index = 0xFFFFFFFF;

    string lower = to_lower_copy(serial);
    if (lower.compare(0, 2, "0x") == 0)
    {
        index = CAssistant::TextToSerial(serial);

        for (size_t i = 0; i < m_List.size(); i++)
        {
            if (m_List[i].GetSerial() == index)
            {
                return &m_List[i];
            }
        }
    }
    else
    {
        index = (uint)atoi(serial.c_str());

        if (index < (uint)m_List.size())
        {
            return &m_List[index];
        }
    }

    return nullptr;
}

} // namespace xuoassist

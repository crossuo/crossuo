// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/trademanager.cpp).

#include "trademanager.h"
#include "../core/world.h"
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

                if (trade.ID1 == id)
                {
                    // the client handler removes the trade box objects; the
                    // assistant drops only its own trade-window record
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
                if (trade.ID1 == id)
                {
                    trade.Check1 = (bool)reader.ReadUInt8();
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

void CTradeManager::OnTradeAction(
    int action, uint serial, uint id1, uint id2, const string &name)
{
    switch ((TRADE_ACTIONS)action)
    {
        case TA_OPEN:
        {
            m_List.push_back(CTradeWindow(serial, id1, id2, name));

            break;
        }
        case TA_CLOSE:
        {
            for (size_t i = 0; i < m_List.size(); i++)
            {
                CTradeWindow &trade = m_List[i];

                if (trade.ID1 == serial)
                {
                    // the client handler removes the trade box objects; the
                    // assistant drops only its own trade-window record
                    m_List.erase(m_List.begin() + i);

                    return;
                }
            }

            Warning(Assistant, "Warning!!! Unknown trade closed\n");

            break;
        }
        case TA_CHECK:
        {
            for (CTradeWindow &trade : m_List)
            {
                if (trade.ID1 == serial)
                {
                    trade.Check1 = (bool)id1;
                    trade.Check2 = (bool)id2;
                    return;
                }
            }

            Warning(Assistant, "Warning!!! Unknown trade changed\n");

            break;
        }
        default:
        {
            // SECURE_TRADE_UPDATEGOLD/UPDATELEDGER (and anything newer) carry
            // no assistant record - the client gump is the only state
            break;
        }
    }
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
            if (m_List[i].Serial == index)
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

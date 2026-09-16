// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// (lists are std::vector).

#pragma once

#include "core/global.h"
#include "model/commonitems/tradewindow.h"

namespace xuoassist {

typedef vector<CTradeWindow> TRADE_LIST;

class CTradeManager
{
private:
    TRADE_LIST m_List;

public:
    CTradeManager();
    virtual ~CTradeManager();

    CTradeManager(const CTradeManager &) = delete;
    CTradeManager &operator=(const CTradeManager &) = delete;

    // trade-round bookkeeping over the client handler's parsed values
    // (the client owns the 0x6F parse; no consume)
    void OnTradeAction(int action, uint32_t serial, uint32_t id1, uint32_t id2, const astr_t &name);

    // the client's 0x6F send sites report the user's trade answer here (was
    // the 0x6F SecureTradingS send tap in the assistant's packetmanager);
    // check is the sender's own check state (the TA_CHECK wire field)
    void OnTradeSent(int action, uint32_t id, bool check);

    void Clear();

    int Count();

    CTradeWindow *GetTrade(const astr_t &serial);
};

extern CTradeManager g_TradeManager;

} // namespace xuoassist

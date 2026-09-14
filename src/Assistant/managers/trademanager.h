// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// (lists are std::vector).

#pragma once

#include "core/global.h"
#include "model/commonitems/tradewindow.h"
#include <xuocore/mappedfile.h> // ::CDataReader (the client data reader)

namespace xuoassist {

typedef vector<CTradeWindow> TRADE_LIST;

class CTradeManager
{
private:
    TRADE_LIST m_List;

public:
    CTradeManager();
    virtual ~CTradeManager();

    bool OnPacketSend(CDataReader &reader);

    // trade-round bookkeeping over the client handler's parsed values
    // (the client owns the 0x6F parse; no consume)
    void OnTradeAction(int action, uint serial, uint id1, uint id2, const string &name);

    void Clear();

    int Count();

    CTradeWindow *GetTrade(const string &serial);
};

extern CTradeManager g_TradeManager;

} // namespace xuoassist


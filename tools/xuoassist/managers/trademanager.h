// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/trademanager.h), Qt-diet
// (QList -> std::vector).

#ifndef XUOASSIST_TRADEMANAGER_H
#define XUOASSIST_TRADEMANAGER_H

#include "core/global.h"
#include "model/commonitems/tradewindow.h"
#include "model/datareader.h"

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

    bool OnPacketReceived(CDataReader &reader);

    void Clear();

    int Count();

    CTradeWindow *GetTrade(const string &serial);
};

extern CTradeManager g_TradeManager;

} // namespace xuoassist

#endif // XUOASSIST_TRADEMANAGER_H

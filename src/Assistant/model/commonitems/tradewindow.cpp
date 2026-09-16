// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/tradewindow.cpp), verbatim.

#include "tradewindow.h"

namespace xuoassist {

CTradeWindow::CTradeWindow()
{
}

CTradeWindow::CTradeWindow(const uint &serial, const uint &id1, const uint &id2, const astr_t &name)
    : Serial(serial)
    , ID1(id1)
    , ID2(id2)
    , Name(name)
{
}

CTradeWindow::~CTradeWindow()
{
}

} // namespace xuoassist

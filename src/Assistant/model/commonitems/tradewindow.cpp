// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/tradewindow.cpp), verbatim.

#include "tradewindow.h"

namespace xuoassist {

CTradeWindow::CTradeWindow()
= default;

CTradeWindow::CTradeWindow(const uint32_t &serial, const uint32_t &id1, const uint32_t &id2, const astr_t &name)
    : Serial(serial)
    , ID1(id1)
    , ID2(id2)
    , Name(name)
{
}

CTradeWindow::~CTradeWindow()
= default;

} // namespace xuoassist

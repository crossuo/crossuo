// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/tradewindow.cpp), verbatim.

#include "tradewindow.h"

namespace xuoassist {

CTradeWindow::CTradeWindow()
{
}

CTradeWindow::CTradeWindow(const uint &serial, const uint &id1, const uint &id2, const string &name)
    : m_Serial(serial)
    , m_ID1(id1)
    , m_ID2(id2)
    , m_Name(name)
{
}

CTradeWindow::~CTradeWindow()
{
}

} // namespace xuoassist

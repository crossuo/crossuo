// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/tradewindow.h): QString -> std::string.

#pragma once

#include "core/global.h"

namespace xuoassist {

class CTradeWindow
{
    SETGET(uint, Serial, 0)
    SETGET(uint, ID1, 0)
    SETGET(uint, ID2, 0)
    SETGET(string, Name, "")
    SETGET(bool, Check1, false)
    SETGET(bool, Check2, false)

public:
    CTradeWindow();
    CTradeWindow(const uint &serial, const uint &id1, const uint &id2, const string &name);
    virtual ~CTradeWindow();
};

} // namespace xuoassist


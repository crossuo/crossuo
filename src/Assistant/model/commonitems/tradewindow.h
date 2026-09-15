// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/tradewindow.h): strings are std::string.

#pragma once

#include "core/global.h"

namespace xuoassist {

class CTradeWindow
{
public:
    uint Serial = 0;
    uint ID1 = 0;
    uint ID2 = 0;
    string Name = "";
    bool Check1 = false;
    bool Check2 = false;

    CTradeWindow();
    CTradeWindow(const uint &serial, const uint &id1, const uint &id2, const string &name);
    virtual ~CTradeWindow();
};

} // namespace xuoassist


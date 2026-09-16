// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/tradewindow.h): strings are astr_t.

#pragma once

#include "core/global.h"

namespace xuoassist {

class CTradeWindow
{
public:
    uint32_t Serial = 0;
    uint32_t ID1 = 0;
    uint32_t ID2 = 0;
    astr_t Name = "";
    bool Check1 = false;
    bool Check2 = false;

    CTradeWindow();
    CTradeWindow(const uint32_t &serial, const uint32_t &id1, const uint32_t &id2, const astr_t &name);
    virtual ~CTradeWindow();
};

} // namespace xuoassist

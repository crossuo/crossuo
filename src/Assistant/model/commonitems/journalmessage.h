// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The journal message record; Timer is a g_Ticks stamp.

#pragma once

#include "core/global.h"

namespace xuoassist {

class CJournalMessage
{
public:
    uint32_t Serial = 0;
    uint32_t Timer = 0;
    uint16_t Color = 0;
    astr_t Text = "";
    uint8_t Flags = 0;
    int FindTextID = 0;

    CJournalMessage();
    CJournalMessage(const CJournalMessage &msg);
    CJournalMessage(const uint32_t &serial, const uint16_t &color, const astr_t &text);
    virtual ~CJournalMessage();
};

} // namespace xuoassist

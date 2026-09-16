// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/journalmessage.h): Timer is a g_Ticks
// stamp (the original app used an elapsed-time stamp).

#pragma once

#include "core/global.h"

namespace xuoassist {

class CJournalMessage
{
public:
    uint Serial = 0;
    uint Timer = 0;
    ushort Color = 0;
    astr_t Text = "";
    uchar Flags = 0;
    int FindTextID = 0;

    CJournalMessage();
    CJournalMessage(const CJournalMessage &msg);
    CJournalMessage(const uint &serial, const ushort &color, const astr_t &text);
    virtual ~CJournalMessage();
};

} // namespace xuoassist

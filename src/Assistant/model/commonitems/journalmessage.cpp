// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/journalmessage.cpp): g_Timer.elapsed()
// is a g_Ticks stamp.

#include "journalmessage.h"

namespace xuoassist {

CJournalMessage::CJournalMessage()
= default;

CJournalMessage::CJournalMessage(const CJournalMessage &msg) = default;

CJournalMessage::CJournalMessage(const uint32_t &serial, const uint16_t &color, const astr_t &text)
    : Serial(serial)
    , Timer(g_Ticks)
    , Color(color)
    , Text(text)
    , Flags(0)
{
}

CJournalMessage::~CJournalMessage()
= default;

} // namespace xuoassist

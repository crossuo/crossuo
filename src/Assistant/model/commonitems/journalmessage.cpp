// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/journalmessage.cpp): g_Timer.elapsed()
// became g_Ticks (plan §4.2).

#include "journalmessage.h"

namespace xuoassist {

CJournalMessage::CJournalMessage()
{
}

CJournalMessage::CJournalMessage(const CJournalMessage &msg)
    : Serial(msg.Serial)
    , Timer(msg.Timer)
    , Color(msg.Color)
    , Text(msg.Text)
    , Flags(msg.Flags)
    , FindTextID(msg.FindTextID)
{
}

CJournalMessage::CJournalMessage(const uint &serial, const ushort &color, const string &text)
    : Serial(serial)
    , Timer(g_Ticks)
    , Color(color)
    , Text(text)
    , Flags(0)
{
}

CJournalMessage::~CJournalMessage()
{
}

} // namespace xuoassist

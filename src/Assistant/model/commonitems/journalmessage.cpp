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
    : m_Serial(msg.GetSerial())
    , m_Timer(msg.GetTimer())
    , m_Color(msg.GetColor())
    , m_Text(msg.GetText())
    , m_Flags(msg.GetFlags())
    , m_FindTextID(msg.GetFindTextID())
{
}

CJournalMessage::CJournalMessage(const uint &serial, const ushort &color, const string &text)
    : m_Serial(serial)
    , m_Timer(g_Ticks)
    , m_Color(color)
    , m_Text(text)
    , m_Flags(0)
{
}

CJournalMessage::~CJournalMessage()
{
}

} // namespace xuoassist

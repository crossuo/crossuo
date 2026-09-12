// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/journalmessage.h): Timer is a g_Ticks
// stamp (the Qt app used g_Timer.elapsed()).

#ifndef XUOASSIST_JOURNALMESSAGE_H
#define XUOASSIST_JOURNALMESSAGE_H

#include "core/global.h"

namespace xuoassist {

class CJournalMessage
{
    SETGET(uint, Serial, 0)
    SETGET(uint, Timer, 0)
    SETGET(ushort, Color, 0)
    SETGET(string, Text, "")
    SETGET(uchar, Flags, 0)
    SETGET(int, FindTextID, 0)

public:
    CJournalMessage();
    CJournalMessage(const CJournalMessage &msg);
    CJournalMessage(const uint &serial, const ushort &color, const string &text);
    virtual ~CJournalMessage();
};

} // namespace xuoassist

#endif // XUOASSIST_JOURNALMESSAGE_H

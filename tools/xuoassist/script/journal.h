// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The script-visible journal mirror (was managers/journalmanager.* in the Qt
// app feeding CScriptEngine::InJournal/WaitJournal/ClearJournal). The Qt
// manager was not part of the Phase 1c port (only the journal event existed),
// so the ring lives in the script module: main-thread fed from the journal
// event consumer (assistanteventsui.cpp), main-thread read inside the command
// queue drain, script threads only ever see copies made at completion time.
//
// Message records are the ported CJournalMessage (Timer = g_Ticks stamp,
// Flags = JMT_* source bits, FindTextID = matched-pattern index) - identical
// fields to the Qt app's entries, so script semantics match.

#include "../core/global.h"
#include "../model/commonitems/journalmessage.h"

namespace xuoassist {

// Source/type flags: the JMT_* enum already lives in core/global.h (ported
// with the Phase 1c journal event payload); do not redefine it here.

// Entry points fed from the assistant events consumer (main thread only).
void ScriptJournal_Add(const CJournalMessage &message);
void ScriptJournal_Clear();

// Mirrors CJournalManager::SetIgnoreCase (the JournalIgnoreCase command).
void ScriptJournal_SetIgnoreCase(bool value);
bool ScriptJournal_IgnoreCase();

// The FindLine/Find port: scans newest-first for any `|`-separated pattern,
// applying the serial/color/type/time filters; sets FindTextID on the hit and
// optionally removes it (the InJournal(pattern, flags, serial, color, start,
// end, consume) overload). Returns a copy for the queue result, or nullptr
// when nothing matched (caller deletes).
CJournalMessage *ScriptJournal_Find(
    const string &pattern,
    uint serial,
    ushort color,
    uchar type,
    uint startTime,
    uint endTime,
    bool remove);

// The journal text ring (oldest first, capped); main-thread readers only.
const vector<CJournalMessage> &ScriptJournal_Messages();
int ScriptJournal_Count();
// JournalLine(index, consume=true)'s remove-after-read half (port extension;
// the Qt JournalLine never removed). Main thread (drain) only.
bool ScriptJournal_RemoveAt(int index);

} // namespace xuoassist


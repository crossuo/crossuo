// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// Main-thread script-state stores of the 4b-2 round (was the tail of
// managers/commandmanager.* in the Qt app):
//   - globals: the SetGlobal/GetGlobal/ClearGlobals map. The Qt map lived in
//     the CCommandManager singleton, so it was ASSISTANT-GLOBAL (shared by
//     every running script, session lifetime, survives script exit) - the
//     port matches that scope; the main-tab globals list UI it fed was never
//     ported.
//   - timers: the TimerExists/SetTimer/Timer/RemoveTimer/ClearTimers family
//     (m_Timers + m_Time.elapsed() there; here g_Ticks plays m_Time).
//   - buffs: the BuffExists name mirror (was g_BuffManager). The ported
//     packet layer surfaces buffs as events (OnBuffAdd/OnBuffRemove), so the
//     ring lives next to the other script mirrors and the events consumer
//     (assistanteventsui.cpp) feeds it on the main thread.
//
// Threading: every entry point is MAIN THREAD ONLY - the script workers
// reach all of this marshaled through the command queue (SC_GLOBAL_SET /
// SC_TIMER / SC_BUFF_EXISTS in apiscript.cpp).

#include "../core/global.h"

namespace xuoassist {

// --- globals ---------------------------------------------------------------

void ScriptGlobal_Set(const string &name, const string &value);
// Returns "" when the name is not set (Qt OnCommandGetGlobal left the result
// string untouched, which was the empty QString default).
string ScriptGlobal_Get(const string &name);
void ScriptGlobal_Clear();

// --- timers ----------------------------------------------------------------

void ScriptTimer_Set(const string &name, int value);
// -1 when the timer does not exist (Qt OnCommandTimer).
int ScriptTimer_Get(const string &name);
bool ScriptTimer_Exists(const string &name);
void ScriptTimer_Remove(const string &name);
void ScriptTimer_Clear();

// --- buffs -----------------------------------------------------------------

void ScriptBuffs_Add(ushort graphic, uint timer, const string &name);
void ScriptBuffs_Remove(ushort graphic);
// Case-insensitive name match (the Qt body trimmed + lowered the script
// argument; the mirror stores lowered names at event time).
bool ScriptBuffs_Exists(const string &name);

} // namespace xuoassist


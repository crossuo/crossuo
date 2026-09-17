// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// Main-thread script-state stores:
//   - globals: the SetGlobal/GetGlobal/ClearGlobals map. ASSISTANT-GLOBAL
//     (shared by every running script, session lifetime, survives script exit);
//   - timers: the TimerExists/SetTimer/Timer/RemoveTimer/ClearTimers family
//     (g_Ticks-based);
//   - buffs: the BuffExists name mirror (the packet layer surfaces buffs as
//     events OnBuffAdd/OnBuffRemove).

#include "../core/global.h"

namespace xuoassist {

// --- globals ---------------------------------------------------------------

void ScriptGlobal_Set(const astr_t &name, const astr_t &value);
// Returns "" when the name is not set.
astr_t ScriptGlobal_Get(const astr_t &name);
void ScriptGlobal_Clear();

// --- timers ----------------------------------------------------------------

void ScriptTimer_Set(const astr_t &name, int value);
// -1 when the timer does not exist (OnCommandTimer).
int ScriptTimer_Get(const astr_t &name);
bool ScriptTimer_Exists(const astr_t &name);
void ScriptTimer_Remove(const astr_t &name);
void ScriptTimer_Clear();

// --- buffs -----------------------------------------------------------------

void ScriptBuffs_Add(uint16_t graphic, uint32_t timer, const astr_t &name);
void ScriptBuffs_Remove(uint16_t graphic);
// Case-insensitive name match (trims + lowercases the script
bool ScriptBuffs_Exists(const astr_t &name);

} // namespace xuoassist

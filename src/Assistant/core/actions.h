// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The hotkey action registry (Phase 3, master plan §7 item 2). Replaces the
// original app's signal-hub dispatch: the table the cb_Action combos offered is
// enumerated here as data (name, per-type action list) and every entry's
// handler calls the Phase-1 core directly - no marshaling layer.
//
// Sources:
//   - s_HotkeyTypeName + PlayHotkeyAction:
//     the type names, the fixed per-type action lists and the executable
//     bodies (attack/use-object/all-names/cast/skill/targeting/bandage/NTS);
//   - TabMacros::UpdateMacroList: the "Play macro" action
//     list follows the user's macro names;
//   - the 0xFC OIPMT_SKILL_LIST / OIPMT_SPELL_LIST handlers fed
//     "Last" + skill/spell names into the cast/use-skill action
//     lists through AssistantUI events (no producer since the
//     plugin-era 0xFC path was removed).
//
// Script entries (HT_SCRIPT) keep their stored name this phase: the script
// engine lands in Phase 4, so the handler is inert (documented delta). The
// external-code entries run through the text-command pipeline when the text
// starts with the configured command prefix (typed "_cmd" commands historically
// arrived as outgoing speech and are now intercepted by AssistantSpeechIntercept
// in core/assist.cpp - the same DoCommand call is reused here); anything else is
// inert with a UI tooltip.

#pragma once

#include "global.h"

namespace xuoassist {

struct CHotkeyEntry;

// The type list (s_HotkeyTypeName names; the HT_* enum itself lives in
// config/assistantconfig.h).
const char *Actions_TypeName(uint type);
uint Actions_TypeCount();

// The action combo contents (Content lists). Fixed for most types; the spell
// and skill lists follow the (currently producer-less) Actions_SetSpellList /
// Actions_SetSkillList feeds, the macro list follows g_AssistantConfig.Macros().
int Actions_ActionCount(uint type);
// Writes into a caller buffer (draw paths build no strings).
void Actions_ActionName(uint type, int action, char *out, size_t size);

// The Data column text (UpdateHotkeyData, hotkeysform.cpp:899). Writes into a
// caller buffer; empty result means the original "Blank hotkey" placeholder.
void Actions_FormatDataText(const CHotkeyEntry &entry, char *out, size_t size);

// The executor side (PlayHotkeyAction): runs the binding's action.
void Actions_Execute(const CHotkeyEntry &entry);
// HT_EXTERNAL_CODE body: text-commands when the text carries the command
// prefix, otherwise a logged no-op (script engine = Phase 4).
void Actions_RunExternalCode(const string &code);

// The 0xFC packet feeds (the hotkey action-list sources).
void Actions_SetSpellList(const vector<vector<string>> &books);
void Actions_SetSkillList(const vector<string> &names);

// The new-target-system serial (was the VKI_NEW_TARGET_SYSTEM_VIRTUAL_INT
// variable: GetInt/SetInt(VKI_NEW_TARGET_SYSTEM_SERIAL) in hotkeysform.cpp).
uint Actions_NewTargetSystemSerial();
void Actions_SetNewTargetSystemSerial(uint serial);

} // namespace xuoassist


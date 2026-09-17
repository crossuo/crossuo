// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The hotkey action registry: the action type table is enumerated here as
// data (name, per-type action list) and every entry's handler calls the
// assistant core directly - no marshaling layer.
//
// Sources:
//   - s_HotkeyTypeName + PlayHotkeyAction:
//     the type names, the fixed per-type action lists and the executable
//     bodies (attack/use-object/all-names/cast/skill/targeting/bandage/NTS);
//    - the "Play macro" action list follows the user's macro names;
//    - the skill/spell list events feed "Last" + skill/spell names into the
//      cast/use-skill action lists through AssistantUI events.
//
// Script entries (HT_SCRIPT) keep their stored name; the action handler is
// inert (scripts run from the Scripts tab and the text-command pipeline). The
// external-code entries run through the text-command pipeline when the text
// starts with the configured command prefix (the same DoCommand call the
// typed "_cmd" speech intercept uses, see AssistantSpeechIntercept in
// core/assist.cpp); anything else is inert with a UI tooltip.

#pragma once

#include "global.h"

namespace xuoassist {

struct CHotkeyEntry;

// The type list (s_HotkeyTypeName names; the HT_* enum itself lives in
// config/assistantconfig.h).
const char *Actions_TypeName(uint32_t type);
uint32_t Actions_TypeCount();

// The action combo contents (Content lists). Fixed for most types; the spell
// and skill lists follow the (currently producer-less) Actions_SetSpellList /
// Actions_SetSkillList feeds, the macro list follows g_AssistantConfig.Macros().
int Actions_ActionCount(uint32_t type);
// Writes into a caller buffer (draw paths build no strings).
void Actions_ActionName(uint32_t type, int action, char *out, size_t size);

// The Data column text (UpdateHotkeyData, hotkeysform.cpp:899). Writes into a
// caller buffer; empty result means the "Blank hotkey" placeholder.
void Actions_FormatDataText(const CHotkeyEntry &entry, char *out, size_t size);

// The executor side (PlayHotkeyAction): runs the binding's action.
void Actions_Execute(const CHotkeyEntry &entry);
// HT_EXTERNAL_CODE body: text-commands when the text carries the command
// prefix, otherwise a logged no-op.
void Actions_RunExternalCode(const astr_t &code);

// The 0xFC packet feeds (the hotkey action-list sources).
void Actions_SetSpellList(const vector<vector<astr_t>> &books);
void Actions_SetSkillList(const vector<astr_t> &names);

// The new-target-system serial (
uint32_t Actions_NewTargetSystemSerial();
void Actions_SetNewTargetSystemSerial(uint32_t serial);

} // namespace xuoassist

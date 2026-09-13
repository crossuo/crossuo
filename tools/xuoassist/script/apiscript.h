// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#ifndef XUOASSIST_SCRIPT_APISCRIPT_H
#define XUOASSIST_SCRIPT_APISCRIPT_H

// The assistant JS API surface (was the Q_INVOKABLE surface of
// CScriptEngine = Client./Orion., CPlayerObject = Player., and the sentinel
// globals, script/scriptengine.h + scripthandle.cpp). Batch 1 covered the
// core Player/Client/Orion methods (movement, speech, objects, targets,
// find/count, journal, dress, skills, spells); batch 2 (Phase 4b-2) added
// the engine classes - gump hook/count/get/select, menu wait/get/select,
// prompt wait/send, trade, shop, the WaitFor* flag family, timers, context
// menu, client macros, the text window (scriptgump.*) and the sandboxed
// file engine (scriptfile.*); the 4b-2 FIX round registered the five
// engine classes (Gump/Menu/GumpHook/ClientMacro/File - they used to alias
// the GameObject class) and completed the wire-now surface:
//   - Option* getters + SetOption* setters, all 35 (the Qt name/VKI
//     crossings kept; CriminalActionsQuery name fixed), gated like Qt
//     (g_World + EnabledCommandOptionsAccess on both directions);
//   - friends/enemies (Find/Get list/status/Add/Remove/Clear), hotkeys
//     (Save/Load/OnOff), UseAbility/UseWrestlingDisarm/Stun/InvokeVirtue,
//     ClientViewRange get/set, SetPVPCaller, the 6 font-color forms,
//     Contains/Split, OpenPaperdoll;
//   - medium batch: ClosePaperdoll/MovePaperdoll, ShowStatusbar/
//     CloseStatusbar, GetTargetType, Screenshot, LogOut.
// Client./Orion. now totals 270 registrations; the class prototypes add
// Gump 18, Menu 11, GumpHook 3, File 9, ClientMacro 2.
// Remaining documented deviations (with reasons, in apiscript.cpp near the
// registration tail): ValidateTargetTile x2 (no in-process tiledata GetTile
// helper), LoadProfile (drives the full UI/profile reload, not queueable),
// SayBroadcast stays a plain say (no speech modes), PlayWav (no audio
// mixer). The ScriptManager/hotkey-form internals stay phase-owned.

#include "../core/global.h"

struct JSContext;

namespace xuoassist {

class CScriptEngine;

// Installs the global objects + sentinel globals on a fresh context (the
// newQObject/globalObject().setProperty block of scripthandle.cpp:127-166).
void Script_RegisterAPI(JSContext *ctx, const string &scriptName);

// Exception -> "lineNumber" + message strings (the QScriptValue
// lineNumber/toString pair of scripthandle.cpp:169-172).
void Script_FormatException(JSContext *ctx, string &line, string &message);

} // namespace xuoassist

#endif // XUOASSIST_SCRIPT_APISCRIPT_H

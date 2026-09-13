// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#ifndef XUOASSIST_SCRIPT_APISCRIPT_H
#define XUOASSIST_SCRIPT_APISCRIPT_H

// The assistant JS API surface (was the Q_INVOKABLE surface of
// CScriptEngine = Client./Orion., CPlayerObject = Player., and the sentinel
// globals, script/scriptengine.h + scripthandle.cpp). Batch 1 covers the
// ~150 core Player/Client/Orion methods (movement, speech, objects, targets,
// find/count, journal, dress, skills, spells); the gump/textwindow/menu/
// prompt engines return the Phase 4b-2 "not yet available" journal message.

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

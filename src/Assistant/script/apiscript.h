// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The assistant JS API surface

#include "../core/global.h"

struct JSContext;

namespace xuoassist {

// The SC_SAY SubTag (apiscript.cpp): the Say prefix kinds index the prefix
// table; SAY_ERROR is the marshaled ScriptError print (engine.cpp) and is
// never a Say prefix.
enum XA_SAY_KIND
{
    SAY_NORMAL = 0,
    SAY_PRINT,
    SAY_CHARPRINT,
    SAY_YELL,
    SAY_WHISPER,
    SAY_EMOTE,
    SAY_BROADCAST,
    SAY_GUILD,
    SAY_ALLIANCE,
    SAY_PARTY,
    SAY_ERROR
};

class CScriptEngine;

// Installs the global objects + sentinel globals on a fresh context
// (installs the global objects).
void Script_RegisterAPI(JSContext *ctx, const astr_t &scriptName);

// Exception -> "lineNumber" + message strings (the
// script-value lineNumber/toString pair).
void Script_FormatException(JSContext *ctx, astr_t &line, astr_t &message);

// Raises the uncatchable "interrupted" error on the engine's context: the
// forced-stop path (Stop pressed while the script sits in a native call). The
// error matches what the QuickJS interrupt handler raises, so the script
// unwinds the same way an interrupt would.
void Script_ReportInterrupted(CScriptEngine *engine);

} // namespace xuoassist

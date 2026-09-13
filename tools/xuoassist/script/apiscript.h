// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The assistant JS API surface

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

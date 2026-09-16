// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// The assistant JS API surface

#include "../core/global.h"

struct JSContext;

namespace xuoassist {

class CScriptEngine;

// Installs the global objects + sentinel globals on a fresh context
// (the old global-object registration block).
void Script_RegisterAPI(JSContext *ctx, const astr_t &scriptName);

// Exception -> "lineNumber" + message strings (the old
// script-value lineNumber/toString pair).
void Script_FormatException(JSContext *ctx, astr_t &line, astr_t &message);

} // namespace xuoassist

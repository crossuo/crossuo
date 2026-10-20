// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

// Macro-to-script conversion: renders a recorded macro action list as
// JavaScript source for the assistant script engine (the Client. table in
// script/apiscript.cpp). Pure text generation - no game state, no engine
// calls; actions without a faithful script binding become commented
// placeholder lines.

#include "../core/global.h"
#include "../macros/macro.h"

namespace xuoassist {

// An empty action list yields a script with just the header comment.
astr_t MacroToScript(const vector<CMacro *> &actions, const astr_t &name);

} // namespace xuoassist

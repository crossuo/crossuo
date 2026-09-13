// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/prompt.h): QString -> std::string.

#pragma once

#include "core/global.h"

namespace xuoassist {

class CPrompt
{
    SETGET(uint, Serial, 0)
    SETGET(string, Text, "")
    SETGET(PROMPT_TYPE, Type, PT_NONE)

public:
    CPrompt() {}
    CPrompt(const uint &serial, const string &text, const PROMPT_TYPE &type);
};

} // namespace xuoassist


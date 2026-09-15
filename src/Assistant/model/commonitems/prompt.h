// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/prompt.h): strings are std::string.

#pragma once

#include "core/global.h"

namespace xuoassist {

class CPrompt
{
public:
    uint Serial = 0;
    string Text = "";
    PROMPT_TYPE Type = PT_NONE;

    CPrompt() {}
    CPrompt(const uint &serial, const string &text, const PROMPT_TYPE &type);
};

} // namespace xuoassist


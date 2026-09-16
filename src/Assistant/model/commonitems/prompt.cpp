// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/prompt.cpp), verbatim.

#include "prompt.h"

namespace xuoassist {

CPrompt::CPrompt(const uint &serial, const astr_t &text, const PROMPT_TYPE &type)
    : Serial(serial)
    , Text(text)
    , Type(type)
{
}

} // namespace xuoassist

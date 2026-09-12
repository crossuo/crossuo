// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/prompt.cpp), verbatim.

#include "prompt.h"

namespace xuoassist {

CPrompt::CPrompt(const uint &serial, const string &text, const PROMPT_TYPE &type)
    : m_Serial(serial)
    , m_Text(text)
    , m_Type(type)
{
}

} // namespace xuoassist

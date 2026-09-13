// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/promptmanager.h), trimmed
// (lists are std::vector).

#pragma once

#include "core/global.h"
#include "model/commonitems/prompt.h"
#include <xuocore/mappedfile.h> // ::CDataReader (the client data reader)

namespace xuoassist {

typedef vector<CPrompt> PROMPT_LIST;

class CPromptManager
{
    SETGET(bool, IsUnicode, false)
    SETGET(bool, Exists, false)
    SETGET(uint, Serial, 0)
    SETGET(uint, ID, 0)

private:
    PROMPT_LIST m_Hooks;

public:
    CPromptManager();

    void AddHook(const CPrompt &prompt);

    void ClearHooks();

    bool OnPacketReceived(CDataReader &reader);

    bool OnPacketSend(CDataReader &reader);

    void Send(string text);
};

extern CPromptManager g_PromptManager;

} // namespace xuoassist


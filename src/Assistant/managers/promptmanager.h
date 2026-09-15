// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// (lists are std::vector).

#pragma once

#include "core/global.h"
#include "model/commonitems/prompt.h"
#include <xuocore/mappedfile.h> // ::CDataReader (the client data reader)

namespace xuoassist {

typedef vector<CPrompt> PROMPT_LIST;

class CPromptManager
{
public:
    bool IsUnicode = false;
    bool Exists = false;
    uint Serial = 0;
    uint ID = 0;

private:
    PROMPT_LIST m_Hooks;

public:
    CPromptManager();

    void AddHook(const CPrompt &prompt);

    void ClearHooks();

    bool OnPacketReceived(CDataReader &reader);

    // the client's 0x9A/0xC2 prompt-response send sites report the answer
    // here (was the 0x9A/0xC2 send taps parsing the text out of the buffer);
    // cancel records an empty answer, like the old response-field check
    void OnPromptSent(const string &text, bool cancel);

    void Send(string text);
};

extern CPromptManager g_PromptManager;

} // namespace xuoassist


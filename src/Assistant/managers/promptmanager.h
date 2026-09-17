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
    uint32_t Serial = 0;
    uint32_t ID = 0;

private:
    PROMPT_LIST m_Hooks;

public:
    CPromptManager();

    CPromptManager(const CPromptManager &) = delete;
    CPromptManager &operator=(const CPromptManager &) = delete;

    void AddHook(const CPrompt &prompt);

    void ClearHooks();

    bool OnPacketReceived(CDataReader &reader);

    // the client's 0x9A/0xC2 prompt-response send sites report the answer
    // here; cancel records an empty answer
    void OnPromptSent(const astr_t &text, bool cancel);

    void Send(astr_t text);
};

extern CPromptManager g_PromptManager;

} // namespace xuoassist

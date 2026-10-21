// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/promptmanager.cpp). The macro wait tap
// (the PlayWaiting port) and the recording branch run through
// g_MacroExecutor.

#include "promptmanager.h"
#include "../core/assist.h"
#include "Network/Packets.h" // the raw prompt-reply builders
#include "../macros/macroexecutor.h"
#include "../core/world.h"
#include "../../Globals.h" // g_ServerLanguageReport
#include <common/str.h>

namespace xuoassist {

CPromptManager g_PromptManager;
CPromptManager::CPromptManager() = default;

void CPromptManager::AddHook(const CPrompt &prompt)
{
    m_Hooks.push_back(prompt);
}

void CPromptManager::ClearHooks()
{
    m_Hooks.clear();
}

bool CPromptManager::OnPacketReceived(CDataReader &reader)
{
    if (g_World == nullptr)
    {
        return true;
    }

    IsUnicode = (*reader.Start == 0xC2);
    Serial = reader.ReadUInt32BE();
    ID = reader.ReadUInt32BE();
    Exists = true;

    // a playing macro waiting on the prompt consumes the packet (the macro
    // replay answers the server itself).
    if (g_MacroExecutor.IsPlaying() && g_MacroExecutor.PlayWaiting({ MT_WAIT_FOR_PROMPT }))
    {
        return false;
    }

    if (!m_Hooks.empty())
    {
        const CPrompt &prompt = m_Hooks.front();

        if (prompt.Type != PT_NONE)
        {
            if (!IsUnicode && prompt.Type == PT_UNICODE)
            {
                return true;
            }
            if (IsUnicode && prompt.Type == PT_ASCII)
            {
                return true;
            }
        }

        if ((prompt.Serial != 0u) && prompt.Serial != Serial)
        {
            return true;
        }

        Send(prompt.Text);

        m_Hooks.erase(m_Hooks.begin());

        return false;
    }

    return true;
}

void CPromptManager::OnPromptSent(const astr_t &text, bool cancel)
{
    if (Exists)
    {
        Exists = false;

        // recording: the answer text is recorded as the MT_WAIT_FOR_PROMPT
        // action value; the value is the console text itself
        if (g_World != nullptr && g_MacroExecutor.IsRecording())
        {
            g_MacroExecutor.Record(
                new CMacroWithStringValue(MT_WAIT_FOR_PROMPT, cancel ? astr_t("") : text));
        }
    }
}

void CPromptManager::Send(astr_t text)
{
    if (Exists)
    {
        bool cancel = ((text.length() == 0) || str_lower(text) == "cancel");

        if (cancel)
        {
            text = "";
        }

        if (IsUnicode)
        {
            ::CPacketUnicodePromptResponse(Serial, ID, wstr_from(text), g_ServerLanguageReport, cancel).Send();
        }
        else
        {
            ::CPacketASCIIPromptResponse(Serial, ID, text, cancel).Send();
        }

        Exists = false;
    }
}

} // namespace xuoassist

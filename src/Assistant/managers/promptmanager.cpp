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
#include <common/str.h>

namespace xuoassist {

static string to_lower_copy(string text)
{
    for (char &c : text)
    {
        c = (char)tolower(c);
    }
    return text;
}

CPromptManager g_PromptManager;

CPromptManager::CPromptManager() {}

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

    // was `g_TabMacros->GetPlaying() && g_TabMacros->PlayWaiting({
    // MT_WAIT_FOR_PROMPT })` -> return false: a playing macro waiting on the
    // prompt consumes the packet (the PlayWaiting semantics).
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

void CPromptManager::OnPromptSent(const string &text, bool cancel)
{
    if (Exists)
    {
        Exists = false;

        // tabmacros recording (promptmanager handler): the answer text is
        // recorded as the MT_WAIT_FOR_PROMPT action value; the value is the
        // console text itself (the old buffer parse read the wide string as
        // big-endian and kept the ascii string's trailing NUL)
        if (g_World != nullptr && g_MacroExecutor.IsRecording())
        {
            g_MacroExecutor.Record(
                new CMacroWithStringValue(MT_WAIT_FOR_PROMPT, cancel ? string("") : text));
        }
    }
}

void CPromptManager::Send(string text)
{
    if (Exists)
    {
        bool cancel = ((text.length() == 0) || to_lower_copy(text) == "cancel");

        if (cancel)
        {
            text = "";
        }

        if (IsUnicode)
        {
            ::CPacketUnicodePromptResponse(Serial, ID, wstr_from(text), "ENU", cancel).Send();
        }
        else
        {
            ::CPacketASCIIPromptResponse(Serial, ID, text, cancel).Send();
        }

        Exists = false;
    }
}

} // namespace xuoassist

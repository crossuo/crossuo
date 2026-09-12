// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/promptmanager.cpp). The macro recording /
// waiting branches keep PORT-TODO(phase-3) markers.

#include "promptmanager.h"
#include "../core/assist.h"
#include "../core/packets.h"
#include "../model/gameobjects/gameworld.h"
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

    m_IsUnicode = (*reader.GetStart() == 0xC2);
    m_Serial = reader.ReadUInt32BE();
    m_ID = reader.ReadUInt32BE();
    m_Exists = true;

    // PORT-TODO(phase-3): was `g_TabMacros->GetPlaying() &&
    // g_TabMacros->PlayWaiting({ MT_WAIT_FOR_PROMPT })` -> return false.

    if (!m_Hooks.empty())
    {
        const CPrompt &prompt = m_Hooks.front();

        if (prompt.GetType() != PT_NONE)
        {
            if (!m_IsUnicode && prompt.GetType() == PT_UNICODE)
            {
                return true;
            }
            if (m_IsUnicode && prompt.GetType() == PT_ASCII)
            {
                return true;
            }
        }

        if ((prompt.GetSerial() != 0u) && prompt.GetSerial() != m_Serial)
        {
            return true;
        }

        Send(prompt.GetText());

        m_Hooks.erase(m_Hooks.begin());

        return false;
    }

    return true;
}

bool CPromptManager::OnPacketSend(CDataReader &reader)
{
    if (m_Exists)
    {
        m_Exists = false;

        // PORT-TODO(phase-3): the recording branch parsed the response text
        // and pushed CMacroWithStringValue(MT_WAIT_FOR_PROMPT, text) into
        // g_TabMacros->AddAction when a macro was recording.

        return true;
    }

    return false;
}

void CPromptManager::Send(string text)
{
    if (m_Exists)
    {
        bool cancel = ((text.length() == 0) || to_lower_copy(text) == "cancel");

        if (cancel)
        {
            text = "";
        }

        if (m_IsUnicode)
        {
            CPacketUnicodePromptResponse(m_Serial, m_ID, wstr_from(text), "ENU", cancel).SendServer();
        }
        else
        {
            CPacketASCIIPromptResponse(m_Serial, m_ID, text, cancel).SendServer();
        }

        m_Exists = false;
    }
}

} // namespace xuoassist

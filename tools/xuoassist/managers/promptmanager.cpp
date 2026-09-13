// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/promptmanager.cpp). The macro wait tap
// (the PlayWaiting port) and the recording branch run through
// g_MacroExecutor.

#include "promptmanager.h"
#include "../core/assist.h"
#include "../core/packets.h"
#include "../macros/macroexecutor.h"
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

    // was `g_TabMacros->GetPlaying() && g_TabMacros->PlayWaiting({
    // MT_WAIT_FOR_PROMPT })` -> return false: a playing macro waiting on the
    // prompt consumes the packet (tabmacros.cpp:1023 semantics).
    if (g_MacroExecutor.IsPlaying() && g_MacroExecutor.PlayWaiting({ MT_WAIT_FOR_PROMPT }))
    {
        return false;
    }

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

        // tabmacros recording (Qt promptmanager): parse the response text
        // the client is about to answer with and record it as the
        // MT_WAIT_FOR_PROMPT action value
        if (g_World != nullptr && g_MacroExecutor.IsRecording())
        {
            int length = (int)reader.GetSize() - 15;
            reader.Move(8);
            uint response = reader.ReadUInt32BE();
            string text = "";

            if (response != 0u)
            {
                if (*reader.GetStart() == 0xC2)
                {
                    reader.Move(4);
                    length = (length - 4) / 2;

                    text = str_from(reader.ReadWString(length));
                }
                else
                {
                    text = reader.ReadString(length);
                }
            }

            g_MacroExecutor.Record(new CMacroWithStringValue(MT_WAIT_FOR_PROMPT, text));
        }

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

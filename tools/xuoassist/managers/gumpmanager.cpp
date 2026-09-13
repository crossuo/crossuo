// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/gumpmanager.cpp). The macro wait tap
// (the PlayWaiting port) and the recording branch run through
// g_MacroExecutor; the text-window InfoGump dump uses the script text
// window (the g_TextDialog port).

#include "gumpmanager.h"
#include "../core/assist.h"
#include "../core/assistantevents.h"
#include "../core/packets.h"
#include "../script/scriptgump.h" // the text window dump (the g_TextDialog port)
#include "../macros/macroexecutor.h"
#include "../model/gameobjects/gameworld.h"
#include "../core/textparser.h"
#include <stdlib.h>
#include <common/logging/logging.h>
#include <common/str.h>

namespace xuoassist {

static vector<string> split_skip_empty(const string &text, char separator)
{
    // Was str.split(" ", QString::SkipEmptyParts).
    vector<string> out;
    string current;
    for (char c : text)
    {
        if (c == separator)
        {
            if (!current.empty())
            {
                out.push_back(current);
                current.clear();
            }
        }
        else
        {
            current += c;
        }
    }
    if (!current.empty())
    {
        out.push_back(current);
    }
    return out;
}

static string to_lower_copy(string text)
{
    for (char &c : text)
    {
        c = (char)tolower(c);
    }
    return text;
}

static int to_int(const string &text) { return atoi(text.c_str()); }

static uint to_uint(const string &text) { return (uint)strtoul(text.c_str(), nullptr, 10); }

CGumpManager g_GumpManager;

CGumpManager::CGumpManager() {}

CGumpManager::~CGumpManager() {}

int CGumpManager::GetGumpCount()
{
    return (int)m_Items.size();
}

CGump *CGumpManager::GetLastGump()
{
    return m_LastGump;
}

CGump *CGumpManager::GetGump(const uint &index)
{
    if (index < (uint)m_Items.size())
    {
        return m_Items[index];
    }

    return nullptr;
}

CGump *CGumpManager::GetGump(const uint &serial, const uint &id)
{
    for (size_t i = 0; i < m_Items.size(); i++)
    {
        if (m_Items[i]->GetSerial() == serial && m_Items[i]->GetID() == id)
        {
            return m_Items[i];
        }
    }

    return nullptr;
}

void CGumpManager::RemoveLastGump()
{
    for (size_t i = 0; i < m_Items.size(); i++)
    {
        if (m_Items[i] == m_LastGump)
        {
            m_LastGump = nullptr;
            break;
        }
    }

    delete m_LastGump;
    m_LastGump = nullptr;
}

void CGumpManager::ClearGumps()
{
    for (size_t i = 0; i < m_Items.size(); i++)
    {
        delete m_Items[i];
    }

    m_Items.clear();
    m_LastGump = nullptr;
}

void CGumpManager::AddHook(CGumpHook *hook)
{
    m_Hooks.push_back(CGumpHook(hook));
}

void CGumpManager::ClearHooks()
{
    m_Hooks.clear();
}

bool CGumpManager::OnPacketReceived(CDataReader &reader)
{
    RemoveLastGump();

    if (g_World == nullptr)
    {
        return true;
    }

    uint serial = reader.ReadUInt32BE();
    uint id = reader.ReadUInt32BE();
    int x = reader.ReadInt32BE();
    int y = reader.ReadInt32BE();

    m_LastGump = new CGump(serial, id, x, y);
    m_Items.push_back(m_LastGump);

    ushort commandsLength = reader.ReadUInt16BE();
    string commands = reader.ReadString(commandsLength);
    CTextParser parser(commands, "", "", "{}");

    vector<string> commandList = parser.ReadTokens();

    for (const string &str : commandList)
    {
        vector<string> list = split_skip_empty(str, ' ');

        if (list.empty())
        {
            continue;
        }

        string cmd = to_lower_copy(list.at(0));

        GUMP_ITEM_TYPE type = GIT_INVALID_TYPE;

        if (cmd == "nodispose")
        {
            m_LastGump->SetNoDispose(true);
            type = GIT_NODISPOSE;
        }
        else if (cmd == "nomove")
        {
            m_LastGump->SetNoMove(true);
            type = GIT_NOMOVE;
        }
        else if (cmd == "noclose")
        {
            m_LastGump->SetNoClose(true);
            type = GIT_NOCLOSE;
        }
        else if (cmd == "page")
        {
            type = GIT_PAGE;
        }
        else if (cmd == "group")
        {
            type = GIT_GROUP;
        }
        else if (cmd == "endgroup")
        {
            type = GIT_GROUP;
        }
        else if (cmd == "resizepic")
        {
            type = GIT_RESIZEPIC;
        }
        else if (cmd == "checkertrans")
        {
            type = GIT_CHECKERTRANS;
        }
        else if (cmd == "button")
        {
            type = GIT_BUTTON;
        }
        else if (cmd == "buttontileart")
        {
            type = GIT_BUTTONTILEART;
        }
        else if (cmd == "checkbox")
        {
            type = GIT_CHECKBOX;
        }
        else if (cmd == "radio")
        {
            type = GIT_RADIO;
        }
        else if (cmd == "text")
        {
            type = GIT_TEXT;
        }
        else if (cmd == "croppedtext")
        {
            type = GIT_CROPPEDTEXT;
        }
        else if (cmd == "textentry")
        {
            type = GIT_TEXTENTRY;
        }
        else if (cmd == "textentrylimited")
        {
            type = GIT_TEXTENTRYLIMITED;
        }
        else if (cmd == "tilepic")
        {
            type = GIT_TILEPIC;
        }
        else if (cmd == "tilepichue")
        {
            type = GIT_TILEPICHUE;
        }
        else if (cmd == "gumppic")
        {
            type = GIT_GUMPPIC;
        }
        else if (cmd == "gumppictiled")
        {
            type = GIT_GUMPPICTILED;
        }
        else if (cmd == "htmlgump")
        {
            type = GIT_HTMLGUMP;
        }
        else if (cmd == "xmfhtmlgump")
        {
            type = GIT_XMFHTMLGUMP;
        }
        else if (cmd == "xmfhtmlgumpcolor")
        {
            type = GIT_XMLHTMLGUMPCOLOR;
        }
        else if (cmd == "xmfhtmltok")
        {
            type = GIT_XMLHTMLTOK;
        }
        else if (cmd == "tooltip")
        {
            type = GIT_TOOLTIP;
        }
        else if (cmd == "mastergump")
        {
            type = GIT_MASTERGUMP;
        }

        if (type != GIT_INVALID_TYPE)
        {
            m_LastGump->m_Items.push_back(CGumpItem(type, str));
        }
    }

    int textLinesCount = reader.ReadInt16BE();

    for (int i = 0; i < textLinesCount; i++)
    {
        int linelen = reader.ReadInt16BE();
        string line = "";

        if (linelen != 0)
        {
            line = str_from(reader.ReadWString(linelen, true));
        }

        m_LastGump->m_Text.push_back(line);
    }

    // was `g_TabMacros->GetPlaying() && g_TabMacros->PlayWaiting({
    // MT_WAIT_FOR_GUMP })` -> return false (tabmacros.cpp:1023): a playing
    // macro waiting on this gump has its wait satisfied and consumes the
    // packet (the macro replay answers the server itself).
    if (g_World != nullptr && g_MacroExecutor.IsPlaying() &&
        g_MacroExecutor.PlayWaiting({ MT_WAIT_FOR_GUMP }))
    {
        return false;
    }

    if (!m_Hooks.empty())
    {
        if (SendGumpChoice(m_LastGump, m_Hooks.front()))
        {
            m_Hooks.erase(m_Hooks.begin());
            return false;
        }
    }

    return true;
}

bool CGumpManager::OnPacketSend(CDataReader &reader)
{
    uint serial = reader.ReadUInt32BE();
    uint id = reader.ReadUInt32BE();

    for (size_t i = 0; i < m_Items.size(); i++)
    {
        CGump *gump = m_Items[i];

        if (!gump->GetReplayed() && gump->GetSerial() == serial && gump->GetID() == id)
        {
            uint button = reader.ReadUInt32BE();

            gump->SetReplayed(true);
            gump->SetReplyID(button);

            // tabmacros recording (Qt gumpmanager): CMacroWithGump(
            // MT_WAIT_FOR_GUMP, serial, id, button)
            if (g_World != nullptr && g_MacroExecutor.IsRecording())
            {
                g_MacroExecutor.Record(new CMacroWithGump(MT_WAIT_FOR_GUMP, serial, id, button));
            }

            if (gump != m_LastGump)
            {
                delete gump;
            }

            m_Items.erase(m_Items.begin() + i);

            break;
        }
    }

    return true;
}

bool CGumpManager::SendGumpChoice(CGump *gump, CGumpHook &hook)
{
    if (gump == nullptr || gump->GetReplayed())
    {
        return false;
    }

    int returnCode = -1;
    int hookIndex = hook.GetIndex();

    if ((hookIndex == 0) && !gump->GetNoClose())
    {
        returnCode = 0;
    }
    else if (!gump->m_Items.empty())
    {
        for (const CGumpItem &item : gump->m_Items)
        {
            if (item.GetType() == GIT_BUTTON || item.GetType() == GIT_BUTTONTILEART)
            {
                vector<string> commands = split_skip_empty(item.GetCommand(), ' ');

                if (commands.size() >= 8 && (to_int(commands[5]) != 0))
                {
                    int itemIndex = to_int(commands[7]);

                    if (hookIndex == itemIndex)
                    {
                        returnCode = itemIndex;

                        g_Assistant.ClientPrint("Gump choice successful.");
                        break;
                    }
                }
            }
        }
    }

    if (returnCode == -1)
    {
        return false;
    }

    if (!hook.m_Checks.empty() || !hook.m_Entries.empty())
    {
        int page = 0;
        int group = 0;

        for (CGumpItem &item : gump->m_Items)
        {
            if (item.GetType() == GIT_PAGE)
            {
                vector<string> commands = split_skip_empty(item.GetCommand(), ' ');

                if (commands.size() >= 2)
                {
                    page = to_int(commands[1]);
                }
            }
            else if (item.GetType() == GIT_GROUP)
            {
                vector<string> commands = split_skip_empty(item.GetCommand(), ' ');

                if (commands.size() >= 2)
                {
                    group = to_int(commands[1]);
                }
                else
                {
                    group = 0;
                }
            }
            else if (
                (item.GetType() == GIT_CHECKBOX || item.GetType() == GIT_RADIO) &&
                !hook.m_Checks.empty())
            {
                vector<string> commands = split_skip_empty(item.GetCommand(), ' ');

                if (commands.size() >= 7)
                {
                    for (size_t i = 0; i < hook.m_Checks.size(); i++)
                    {
                        CGumpHookCheck &checkHook = hook.m_Checks[i];

                        if (to_int(commands[6]) == checkHook.GetIndex())
                        {
                            if (item.GetType() == GIT_RADIO)
                            {
                                int usedPage = 0;
                                int usedGroup = 0;

                                for (CGumpItem &tempItem : gump->m_Items)
                                {
                                    if (tempItem.GetType() == GIT_RADIO)
                                    {
                                        vector<string> tempCommands =
                                            split_skip_empty(tempItem.GetCommand(), ' ');

                                        if (tempCommands.size() >= 7 &&
                                            ((usedPage <= 1 && page <= 1) || (usedPage == page)))
                                        {
                                            if (usedGroup == group)
                                            {
                                                tempCommands[5] = "0";

                                                tempItem.RestoreCommand(tempCommands);
                                            }
                                        }
                                    }
                                    else if (tempItem.GetType() == GIT_PAGE)
                                    {
                                        vector<string> tempCommands =
                                            split_skip_empty(tempItem.GetCommand(), ' ');

                                        if (tempCommands.size() >= 2)
                                        {
                                            usedPage = to_int(tempCommands[1]);
                                        }
                                    }
                                    else if (tempItem.GetType() == GIT_GROUP)
                                    {
                                        vector<string> tempCommands =
                                            split_skip_empty(tempItem.GetCommand(), ' ');

                                        if (tempCommands.size() >= 2)
                                        {
                                            usedGroup = to_int(tempCommands[1]);
                                        }
                                        else
                                        {
                                            group = 0;
                                        }
                                    }
                                }
                            }

                            commands[5] = (checkHook.GetState() ? "1" : "0");
                            item.RestoreCommand(commands);

                            hook.m_Checks.erase(hook.m_Checks.begin() + i);
                            break;
                        }
                    }
                }
            }
            else if (
                (item.GetType() == GIT_TEXTENTRY || item.GetType() == GIT_TEXTENTRYLIMITED) &&
                !hook.m_Entries.empty())
            {
                vector<string> commands = split_skip_empty(item.GetCommand(), ' ');

                if (commands.size() >= 8)
                {
                    int maxLength = 0;

                    if (item.GetType() == GIT_TEXTENTRYLIMITED && commands.size() >= 9)
                    {
                        maxLength = to_int(commands[8]);
                    }

                    for (size_t i = 0; i < hook.m_Entries.size(); i++)
                    {
                        CGumpHookEntry &entryHook = hook.m_Entries[i];

                        if (to_int(commands[6]) == entryHook.GetIndex())
                        {
                            uint textIndex = to_uint(commands[7]);

                            if (textIndex < (uint)gump->m_Text.size())
                            {
                                string text = entryHook.GetText();

                                if ((maxLength != 0) && text.length() > (size_t)maxLength)
                                {
                                    text.resize(maxLength);
                                }

                                gump->m_Text[textIndex] = text;

                                hook.m_Entries.erase(hook.m_Entries.begin() + i);

                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    CPacketGumpResponse(gump, returnCode).SendServer();
    CPacketCloseGenericGumpWithoutResponse(gump, true).SendClient();

    gump->SetReplayed(true);
    gump->SetReplyID(returnCode);

    return true;
}

void CGumpManager::InfoGump(const string &gumpIndex)
{
    int index = -1;

    if (to_lower_copy(gumpIndex) != "lastgump" && to_lower_copy(gumpIndex) != "last")
    {
        index = to_int(gumpIndex);
    }

    CGump *gump = nullptr;

    if (index == -1)
    {
        gump = m_LastGump;
    }
    else if (index >= 0 && index < (int)m_Items.size())
    {
        gump = m_Items[index];
    }

    if (gump == nullptr)
    {
        return;
    }

    // Was g_TextDialog->ClearText()/AddText(...) + show/activateWindow -
    // restored by the 4b-2 text window (scriptgump.cpp); the same
    // information goes through the journal-message seam as well.
    string text = "";

    char buf[128] = { 0 };
    snprintf(
        buf,
        sizeof(buf),
        "Gump information:\nSerial=0x%08X\nID=0x%08X\nX=%i\nY=%i\n",
        gump->GetSerial(),
        gump->GetID(),
        gump->GetX(),
        gump->GetY());
    text += buf;

    if (gump->GetReplayed())
    {
        text += "\nGump replayed with id: " + std::to_string(gump->GetReplyID()) + "\n";
    }

    text += "\nGump buttons:";
    for (const CGumpItem &item : gump->m_Items)
    {
        if (item.GetType() == GIT_BUTTON || item.GetType() == GIT_BUTTONTILEART)
        {
            vector<string> commands = split_skip_empty(item.GetCommand(), ' ');

            if (commands.size() >= 8)
            {
                text += "\n[" + commands[7] + "]\t" + item.GetCommand();
            }
        }
    }

    text += "\n\nGump checks & radios:";
    for (const CGumpItem &item : gump->m_Items)
    {
        if (item.GetType() == GIT_CHECKBOX || item.GetType() == GIT_RADIO)
        {
            vector<string> commands = split_skip_empty(item.GetCommand(), ' ');

            if (commands.size() >= 7)
            {
                text += "\n[" + commands[6] + "]\t" + item.GetCommand();
            }
        }
    }

    text += "\n\nGump entries:";
    for (const CGumpItem &item : gump->m_Items)
    {
        if (item.GetType() == GIT_TEXTENTRY || item.GetType() == GIT_TEXTENTRYLIMITED)
        {
            vector<string> commands = split_skip_empty(item.GetCommand(), ' ');

            if (commands.size() >= 8)
            {
                text += "\n[" + commands[6] + "]\t" + item.GetCommand();
            }
        }
    }

    text += "\n\nGump all commands:";
    for (const CGumpItem &item : gump->m_Items) { text += "\n" + item.GetCommand(); }

    index = 0;
    text += "\n\nGump text:";
    for (const string &str : gump->m_Text)
    {
        text += "\n[" + std::to_string(index) + "]\t" + str;
        index++;
    }

    CJournalMessage message(0xFFFFFFFF, g_MessagesColor, text);
    message.SetFlags(JMT_SYSTEM);
    g_AssistantEvents->OnJournalMessage(message);

    // the g_TextDialog->ClearText/AddText + show port (scriptgump.cpp)
    ScriptTextWindow_Command(TW_CLEAR, "", 0, 0, "");
    ScriptTextWindow_Command(TW_PRINT, text, 0, 0, "");
    ScriptTextWindow_Command(TW_OPEN, "", 0, 0, "");
}

} // namespace xuoassist

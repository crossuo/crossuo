// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/gumpmanager.cpp). The macro wait tap
// (the PlayWaiting port) and the recording branch run through
// g_MacroExecutor; the text-window InfoGump dump uses the script text
// window (the g_TextDialog port).

#include "gumpmanager.h"
#include "Managers/GumpManager.h" // ::g_GumpManager (client gump marks/updates)
#include "../core/assist.h"
#include "../core/assistantevents.h"
#include "Network/Packets.h" // the raw 0xB1 reply builder
#include "../script/scriptgump.h" // the text window dump (the g_TextDialog port)
#include "../macros/macroexecutor.h"
#include "../core/world.h"
#include <stdlib.h>
#include <common/logging/logging.h>
#include <common/str.h>

namespace xuoassist {

static vector<astr_t> split_skip_empty(const astr_t &text, char separator)
{
    // Was str.split on spaces with empty parts skipped.
    vector<astr_t> out;
    astr_t current;
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


static int to_int(const astr_t &text) { return atoi(text.c_str()); }
static uint to_uint(const astr_t &text) { return (uint)strtoul(text.c_str(), nullptr, 10); }

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
        if (m_Items[i]->Serial == serial && m_Items[i]->ID == id)
        {
            return m_Items[i];
        }
    }

    return nullptr;
}

void CGumpManager::AddGump(uint serial, int x, int y, uint id)
{
    m_LastGump = new CGump(serial, id, x, y);
    m_Items.push_back(m_LastGump);
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

// dispatches on the lower-cased command word (cmd) but records the full
// command line (line) so replay/hook responses can parse the numeric fields
void CGumpManager::SetLastGumpType(const astr_t &cmd, const astr_t &line)
{
    GUMP_ITEM_TYPE type = GIT_INVALID_TYPE;
    if (cmd == "nodispose")
    {
        m_LastGump->NoDispose = true;
        type = GIT_NODISPOSE;
    }
    else if (cmd == "nomove")
    {
        m_LastGump->NoMove = true;
        type = GIT_NOMOVE;
    }
    else if (cmd == "noclose")
    {
        m_LastGump->NoClose = true;
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
        m_LastGump->m_Items.push_back(CGumpItem(type, line));
    }
}

void CGumpManager::SetLastGumpText(const wstr_t &text)
{
    m_LastGump->m_Text.push_back(str_from(text));
}

bool CGumpManager::Consume()
{
    // a playing macro waiting on this gump has its wait satisfied and
    // consumes the gump (the macro replay answers the server itself).
    if (g_World != nullptr && g_MacroExecutor.IsPlaying() &&
        g_MacroExecutor.PlayWaiting({ MT_WAIT_FOR_GUMP }))
    {
        return true;
    }

    if (!m_Hooks.empty())
    {
        if (SendGumpChoice(m_LastGump, m_Hooks.front()))
        {
            m_Hooks.erase(m_Hooks.begin());
            return true;
        }
    }

    return false;
}

void CGumpManager::OnGumpChoiceSent(uint serial, uint id, uint button)
{
    if (g_World == nullptr)
    {
        return;
    }

    for (size_t i = 0; i < m_Items.size(); i++)
    {
        CGump *gump = m_Items[i];

        if (!gump->Replayed && gump->Serial == serial && gump->ID == id)
        {
            gump->Replayed = true;
            gump->ReplyID = button;

            // tabmacros recording (gumpmanager handler): CMacroWithGump(
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
}

bool CGumpManager::SendGumpChoice(CGump *gump, CGumpHook &hook)
{
    if (gump == nullptr || gump->Replayed)
    {
        return false;
    }

    int returnCode = -1;
    int hookIndex = hook.Index;

    if ((hookIndex == 0) && !gump->NoClose)
    {
        returnCode = 0;
    }
    else if (!gump->m_Items.empty())
    {
        for (const CGumpItem &item : gump->m_Items)
        {
            if (item.Type == GIT_BUTTON || item.Type == GIT_BUTTONTILEART)
            {
                vector<astr_t> commands = split_skip_empty(item.Command, ' ');

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
            if (item.Type == GIT_PAGE)
            {
                vector<astr_t> commands = split_skip_empty(item.Command, ' ');

                if (commands.size() >= 2)
                {
                    page = to_int(commands[1]);
                }
            }
            else if (item.Type == GIT_GROUP)
            {
                vector<astr_t> commands = split_skip_empty(item.Command, ' ');

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
                (item.Type == GIT_CHECKBOX || item.Type == GIT_RADIO) &&
                !hook.m_Checks.empty())
            {
                vector<astr_t> commands = split_skip_empty(item.Command, ' ');

                if (commands.size() >= 7)
                {
                    for (size_t i = 0; i < hook.m_Checks.size(); i++)
                    {
                        CGumpHookCheck &checkHook = hook.m_Checks[i];

                        if (to_int(commands[6]) == checkHook.Index)
                        {
                            if (item.Type == GIT_RADIO)
                            {
                                int usedPage = 0;
                                int usedGroup = 0;

                                for (CGumpItem &tempItem : gump->m_Items)
                                {
                                    if (tempItem.Type == GIT_RADIO)
                                    {
                                        vector<astr_t> tempCommands =
                                            split_skip_empty(tempItem.Command, ' ');

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
                                    else if (tempItem.Type == GIT_PAGE)
                                    {
                                        vector<astr_t> tempCommands =
                                            split_skip_empty(tempItem.Command, ' ');

                                        if (tempCommands.size() >= 2)
                                        {
                                            usedPage = to_int(tempCommands[1]);
                                        }
                                    }
                                    else if (tempItem.Type == GIT_GROUP)
                                    {
                                        vector<astr_t> tempCommands =
                                            split_skip_empty(tempItem.Command, ' ');

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

                            commands[5] = (checkHook.State ? "1" : "0");
                            item.RestoreCommand(commands);

                            hook.m_Checks.erase(hook.m_Checks.begin() + i);
                            break;
                        }
                    }
                }
            }
            else if (
                (item.Type == GIT_TEXTENTRY || item.Type == GIT_TEXTENTRYLIMITED) &&
                !hook.m_Entries.empty())
            {
                vector<astr_t> commands = split_skip_empty(item.Command, ' ');

                if (commands.size() >= 8)
                {
                    int maxLength = 0;

                    if (item.Type == GIT_TEXTENTRYLIMITED && commands.size() >= 9)
                    {
                        maxLength = to_int(commands[8]);
                    }

                    for (size_t i = 0; i < hook.m_Entries.size(); i++)
                    {
                        CGumpHookEntry &entryHook = hook.m_Entries[i];

                        if (to_int(commands[6]) == entryHook.Index)
                        {
                            uint textIndex = to_uint(commands[7]);

                            if (textIndex < (uint)gump->m_Text.size())
                            {
                                astr_t text = entryHook.Text;

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

    // Marshal the assistant gump model into the raw 0xB1 reply (the write
    // sequence of the old CPacketGumpResponse builder): checked checkboxes and
    // radios (commands[5] != 0) contribute their commands[6] serial, text
    // entries (commands[7] = text line index) contribute commands[6] as the
    // entry id and the referenced line as the text.
    vector<uint32_t> switches;
    vector<std::pair<uint16_t, astr_t>> textLines;

    for (CGumpItem &item : gump->m_Items)
    {
        switch (item.Type)
        {
            case GIT_CHECKBOX:
            case GIT_RADIO:
            {
                vector<astr_t> commands = split_skip_empty(item.Command, ' ');

                if (commands.size() >= 7 && (atoi(commands[5].c_str()) != 0))
                {
                    switches.push_back((uint32_t)atoi(commands[6].c_str()));
                }

                break;
            }
            case GIT_TEXTENTRY:
            case GIT_TEXTENTRYLIMITED:
            {
                vector<astr_t> commands = split_skip_empty(item.Command, ' ');

                if (commands.size() >= 8)
                {
                    uint textIndex = (uint)strtoul(commands[7].c_str(), nullptr, 10);

                    if (textIndex < (uint)gump->m_Text.size())
                    {
                        textLines.push_back(
                            std::make_pair((uint16_t)atoi(commands[6].c_str()), gump->m_Text[textIndex]));
                    }
                }

                break;
            }
            default:
                break;
        }
    }

    ::CPacketGumpResponse(gump->Serial, gump->ID, (uint32_t)returnCode, switches, textLines)
        .Send();
    // close the client's gumps without answering the server again (the 0xB1
    // response above is the answer)
    ::g_GumpManager.CloseGenericGumps(gump->Serial, gump->ID, true);

    gump->Replayed = true;
    gump->ReplyID = returnCode;

    return true;
}

void CGumpManager::InfoGump(const astr_t &gumpIndex)
{
    int index = -1;

    if (str_lower(gumpIndex) != "lastgump" && str_lower(gumpIndex) != "last")
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
    astr_t text = "";

    char buf[128] = { 0 };
    snprintf(
        buf,
        sizeof(buf),
        "Gump information:\nSerial=0x%08X\nID=0x%08X\nX=%i\nY=%i\n",
        gump->Serial,
        gump->ID,
        gump->X,
        gump->Y);
    text += buf;

    if (gump->Replayed)
    {
        text += "\nGump replayed with id: " + str_from(gump->ReplyID) + "\n";
    }

    text += "\nGump buttons:";
    for (const CGumpItem &item : gump->m_Items)
    {
        if (item.Type == GIT_BUTTON || item.Type == GIT_BUTTONTILEART)
        {
            vector<astr_t> commands = split_skip_empty(item.Command, ' ');

            if (commands.size() >= 8)
            {
                text += "\n[" + commands[7] + "]\t" + item.Command;
            }
        }
    }

    text += "\n\nGump checks & radios:";
    for (const CGumpItem &item : gump->m_Items)
    {
        if (item.Type == GIT_CHECKBOX || item.Type == GIT_RADIO)
        {
            vector<astr_t> commands = split_skip_empty(item.Command, ' ');

            if (commands.size() >= 7)
            {
                text += "\n[" + commands[6] + "]\t" + item.Command;
            }
        }
    }

    text += "\n\nGump entries:";
    for (const CGumpItem &item : gump->m_Items)
    {
        if (item.Type == GIT_TEXTENTRY || item.Type == GIT_TEXTENTRYLIMITED)
        {
            vector<astr_t> commands = split_skip_empty(item.Command, ' ');

            if (commands.size() >= 8)
            {
                text += "\n[" + commands[6] + "]\t" + item.Command;
            }
        }
    }

    text += "\n\nGump all commands:";
    for (const CGumpItem &item : gump->m_Items) { text += "\n" + item.Command; }

    index = 0;
    text += "\n\nGump text:";
    for (const astr_t &str : gump->m_Text)
    {
        text += "\n[" + str_from(index) + "]\t" + str;
        index++;
    }

    CJournalMessage message(0xFFFFFFFF, g_MessagesColor, text);
    message.Flags = JMT_SYSTEM;
    g_AssistantEvents->OnJournalMessage(message);

    // the g_TextDialog->ClearText/AddText + show port (scriptgump.cpp)
    ScriptTextWindow_Command(TW_CLEAR, "", 0, 0, "");
    ScriptTextWindow_Command(TW_PRINT, text, 0, 0, "");
    ScriptTextWindow_Command(TW_OPEN, "", 0, 0, "");
}

} // namespace xuoassist

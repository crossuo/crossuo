// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/menumanager.cpp). The macro wait tap
// (the PlayWaiting port) and the recording branch run through
// g_MacroExecutor; the text-window InfoMenu dump uses the script text
// window (the g_TextDialog port).

#include "menumanager.h"
#include "../core/assist.h"
#include "../core/assistantevents.h"
#include "../core/packets.h"
#include "../script/scriptgump.h" // the text window dump (the g_TextDialog port)
#include "../macros/macroexecutor.h"
#include "../model/gameobjects/gameworld.h"
#include <stdlib.h>
#include <common/logging/logging.h>

namespace xuoassist {

static string to_lower_copy(string text)
{
    for (char &c : text)
    {
        c = (char)tolower(c);
    }
    return text;
}

CMenuManager g_MenuManager;

CMenuManager::CMenuManager() {}

CMenuManager::~CMenuManager() {}

int CMenuManager::GetMenuCount()
{
    return (int)m_Items.size();
}

CMenu *CMenuManager::GetLastMenu()
{
    return m_LastMenu;
}

CMenu *CMenuManager::GetMenu(const string &name)
{
    for (size_t i = 0; i < m_Items.size(); i++)
    {
        // indexOf(name, 0, CaseInsensitive) == 0 -> starts-with, case-insensitive
        const string itemNameLower = to_lower_copy(m_Items[i]->GetName());
        if (itemNameLower.size() >= name.size() &&
            itemNameLower.compare(0, name.size(), to_lower_copy(name)) == 0)
        {
            return m_Items[i];
        }
    }

    uint index = (uint)atoi(name.c_str());

    if (index < (uint)m_Items.size())
    {
        return m_Items[index];
    }

    return nullptr;
}

void CMenuManager::RemoveLastMenu()
{
    for (size_t i = 0; i < m_Items.size(); i++)
    {
        if (m_Items[i] == m_LastMenu)
        {
            m_LastMenu = nullptr;
            break;
        }
    }

    delete m_LastMenu;
    m_LastMenu = nullptr;
}

void CMenuManager::ClearMenus()
{
    for (size_t i = 0; i < m_Items.size(); i++)
    {
        delete m_Items[i];
    }

    m_Items.clear();
    m_LastMenu = nullptr;
}

void CMenuManager::SetHook(const string &name, const string &itemName)
{
    m_Hooks.push_back(CMenuHook(name, itemName));
}

void CMenuManager::ClearHooks()
{
    m_Hooks.clear();
}

bool CMenuManager::OnPacketReceived(CDataReader &reader)
{
    RemoveLastMenu();

    if (g_World == nullptr)
    {
        return true;
    }

    uint serial = reader.ReadUInt32BE();
    ushort id = reader.ReadUInt16BE();

    uchar nameLen = reader.ReadUInt8();
    string name = reader.ReadString(nameLen);

    uchar count = reader.ReadUInt8();

    bool grayMenu = (unpack16(reader.GetPtr()) == 0); //menu/gray menu

    m_LastMenu = new CMenu(serial, id, name, grayMenu);
    m_Items.push_back(m_LastMenu);

    for (int i = 0; i < count; i++)
    {
        ushort graphic = reader.ReadUInt16BE();
        ushort color = reader.ReadUInt16BE();

        nameLen = reader.ReadUInt8();
        name = reader.ReadString(nameLen);

        m_LastMenu->m_Items.push_back(CMenuItem(i + 1, graphic, color, name));
    }

    // was `g_TabMacros->GetPlaying() && g_TabMacros->PlayWaiting({
    // MT_WAIT_FOR_MENU })` -> return false: a playing macro waiting on this
    // menu consumes the packet (tabmacros.cpp:1023 semantics).
    if (g_World != nullptr && g_MacroExecutor.IsPlaying() &&
        g_MacroExecutor.PlayWaiting({ MT_WAIT_FOR_MENU }))
    {
        return false;
    }

    if (!m_Hooks.empty())
    {
        const CMenuHook &hook = m_Hooks.front();

        if (SendChoiceIfMenuExists(hook.GetName(), hook.GetItemName(), true))
        {
            m_Hooks.erase(m_Hooks.begin());
            return false;
        }
    }

    return true;
}

bool CMenuManager::OnPacketSend(CDataReader &reader)
{
    uint serial = reader.ReadUInt32BE();
    uint id = reader.ReadUInt16BE();

    for (size_t i = 0; i < m_Items.size(); i++)
    {
        CMenu *menu = m_Items[i];

        if (!menu->GetReplayed() && menu->GetSerial() == serial && menu->GetID() == id)
        {
            uint index = reader.ReadUInt16BE();

            menu->SetReplayed(true);
            menu->SetReplyID(index);

            // tabmacros recording (Qt menumanager): CMacroWithMenu(
            // MT_WAIT_FOR_MENU, menu name, item name, item graphic, item
            // color) for the chosen item
            if (g_World != nullptr && g_MacroExecutor.IsRecording())
            {
                for (const CMenuItem &item : menu->m_Items)
                {
                    if (item.GetID() == index)
                    {
                        g_MacroExecutor.Record(
                            new CMacroWithMenu(
                                MT_WAIT_FOR_MENU,
                                menu->GetName(),
                                item.GetName(),
                                item.GetGraphic(),
                                item.GetColor()));
                        break;
                    }
                }
            }

            if (menu != m_LastMenu)
            {
                delete menu;
            }

            m_Items.erase(m_Items.begin() + i);

            break;
        }
    }

    return true;
}

bool CMenuManager::SendChoiceIfMenuExists(
    const string &name, const string &itemName, const bool &sendPacket)
{
    for (size_t i = 0; i < m_Items.size(); i++)
    {
        CMenu *menu = m_Items[i];

        if (!menu->GetReplayed() &&
            to_lower_copy(menu->GetName()).size() >= name.size() &&
            to_lower_copy(menu->GetName()).compare(0, name.size(), to_lower_copy(name)) == 0)
        {
            if (SendMenuChoice(menu, itemName, sendPacket))
            {
                if (menu != m_LastMenu)
                {
                    delete menu;
                }

                m_Items.erase(m_Items.begin() + i);

                return true;
            }
        }
    }

    return false;
}

bool CMenuManager::SendMenuChoice(CMenu *menu, const string &itemName, const bool &sendPacket)
{
    if ((itemName.length() == 0) || to_lower_copy(itemName) == "cancel")
    {
        if (sendPacket)
        {
            CPacketMenuResponse(menu->GetSerial(), menu->GetID(), 0, 0, 0).SendServer();

            menu->SetReplayed(true);
            menu->SetReplyID(0);
        }
        else
        {
            CPacketSelectMenuInClient(menu->GetSerial(), menu->GetID(), 0).SendClient();
        }

        return true;
    }

    for (const CMenuItem &item : menu->m_Items)
    {
        // indexOf(itemName) == 0 -> starts-with
        if (item.GetName().size() >= itemName.size() &&
            item.GetName().compare(0, itemName.size(), itemName) == 0)
        {
            if (sendPacket)
            {
                if (menu->GetGrayMenu())
                {
                    CPacketGrayMenuResponse(menu->GetSerial(), menu->GetID(), item.GetID())
                        .SendServer();
                }
                else
                {
                    CPacketMenuResponse(
                        menu->GetSerial(), menu->GetID(), item.GetID(), item.GetGraphic(), item.GetColor())
                        .SendServer();
                }

                menu->SetReplayed(true);
                menu->SetReplyID(item.GetID());
            }
            else
            {
                CPacketSelectMenuInClient(menu->GetSerial(), menu->GetID(), item.GetID()).SendClient();
            }

            return true;
        }
    }

    bool ok = false;
    uint index = 0;

    if (to_lower_copy(itemName) == "random" && !menu->m_Items.empty())
    {
        ok = true;
        index = rand() % menu->m_Items.size();
    }
    else
    {
        index = (uint)atoi(itemName.c_str());
        ok = (index != 0 || itemName == "0");
    }

    if (ok && index < (uint)menu->m_Items.size())
    {
        CMenuItem &item = menu->m_Items[index];

        if (sendPacket)
        {
            if (menu->GetGrayMenu())
            {
                CPacketGrayMenuResponse(menu->GetSerial(), menu->GetID(), item.GetID()).SendServer();
            }
            else
            {
                CPacketMenuResponse(
                    menu->GetSerial(), menu->GetID(), item.GetID(), item.GetGraphic(), item.GetColor())
                    .SendServer();
            }

            menu->SetReplayed(true);
            menu->SetReplyID(item.GetID());
        }
        else
        {
            CPacketSelectMenuInClient(menu->GetSerial(), menu->GetID(), item.GetID()).SendClient();
        }

        return true;
    }

    return false;
}

void CMenuManager::InfoMenu(const string &menuIndex)
{
    int index = -1;

    if (to_lower_copy(menuIndex) != "lastmenu" || to_lower_copy(menuIndex) != "last")
    {
        index = atoi(menuIndex.c_str());
    }

    CMenu *menu = nullptr;

    if (index == -1)
    {
        menu = m_LastMenu;
    }
    else if (index >= 0 && index < (int)m_Items.size())
    {
        menu = m_Items[index];
    }

    if (menu == nullptr)
    {
        return;
    }

    // Was g_TextDialog->ClearText()/AddText(...) + show/activateWindow -
    // restored by the 4b-2 text window (scriptgump.cpp); the parse below
    // matches the original text pieces.
    string text = menu->GetGrayMenu() ? "Gray menu information:\nName: " : "Menu information:\nName: ";
    text += menu->GetName();

    if (menu->GetReplayed())
    {
        text += "\nMenu replayed with id: " + std::to_string(menu->GetReplyID());
    }

    for (const CMenuItem &item : menu->m_Items)
    {
        char buf[256] = { 0 };
        snprintf(
            buf,
            sizeof(buf),
            "\nItem: ID=%i Graphic=0x%04X Color=0x%04X Name=%s",
            item.GetID(),
            item.GetGraphic(),
            item.GetColor(),
            item.GetName().c_str());
        text += buf;
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

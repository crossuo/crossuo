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
#include "../core/world.h"
#include <stdlib.h>
#include <common/logging/logging.h>
#include <algorithm>
#include "Managers/GumpManager.h" // ::g_GumpManager (the client-side menu selection)

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
        const string itemNameLower = to_lower_copy(m_Items[i]->Name);
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

bool CMenuManager::OnMenuReceived(
    uint serial, ushort id, const string &name, bool grayMenu, const vector<CMenuItem> &items)
{
    RemoveLastMenu();

    if (g_World == nullptr)
    {
        return true;
    }

    m_LastMenu = new CMenu(serial, id, name, grayMenu);
    m_Items.push_back(m_LastMenu);

    for (const CMenuItem &item : items)
    {
        m_LastMenu->m_Items.push_back(item);
    }

    // was `g_TabMacros->GetPlaying() && g_TabMacros->PlayWaiting({
    // MT_WAIT_FOR_MENU })` -> return false: a playing macro waiting on this
    // menu consumes the packet (the PlayWaiting semantics).
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

void CMenuManager::OnMenuChoiceSent(uint serial, ushort id, ushort index)
{
    for (size_t i = 0; i < m_Items.size(); i++)
    {
        CMenu *menu = m_Items[i];

        if (!menu->Replayed && menu->Serial == serial && menu->ID == id)
        {
            menu->Replayed = true;
            menu->ReplyID = index;

            // tabmacros recording (menumanager handler): CMacroWithMenu(
            // MT_WAIT_FOR_MENU, menu name, item name, item graphic, item
            // color) for the chosen item
            if (g_World != nullptr && g_MacroExecutor.IsRecording())
            {
                for (const CMenuItem &item : menu->m_Items)
                {
                    if (item.ID == index)
                    {
                        g_MacroExecutor.Record(
                            new CMacroWithMenu(
                                MT_WAIT_FOR_MENU,
                                menu->Name,
                                item.Name,
                                item.Graphic,
                                item.Color));
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
}

bool CMenuManager::SendChoiceIfMenuExists(
    const string &name, const string &itemName, const bool &sendPacket)
{
    for (size_t i = 0; i < m_Items.size(); i++)
    {
        CMenu *menu = m_Items[i];

        if (!menu->Replayed &&
            to_lower_copy(menu->Name).size() >= name.size() &&
            to_lower_copy(menu->Name).compare(0, name.size(), to_lower_copy(name)) == 0)
        {
            if (SendMenuChoice(menu, itemName, sendPacket))
            {
                // the client-side selection (sendPacket == false) runs
                // synchronously: OnMenuChoiceSent may have already replayed
                // and removed the record, so only clean it up when it is
                // still listed
                if (std::find(m_Items.begin(), m_Items.end(), menu) != m_Items.end())
                {
                    if (menu != m_LastMenu)
                    {
                        delete menu;
                    }

                    m_Items.erase(m_Items.begin() + i);
                }

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
            CPacketMenuResponse(menu->Serial, menu->ID, 0, 0, 0).SendServer();

            menu->Replayed = true;
            menu->ReplyID = 0;
        }
        else
        {
            ::g_GumpManager.SelectMenu(menu->Serial, menu->ID, 0);
        }

        return true;
    }

    for (const CMenuItem &item : menu->m_Items)
    {
        // indexOf(itemName) == 0 -> starts-with
        if (item.Name.size() >= itemName.size() &&
            item.Name.compare(0, itemName.size(), itemName) == 0)
        {
            if (sendPacket)
            {
                if (menu->GrayMenu)
                {
                    CPacketGrayMenuResponse(menu->Serial, menu->ID, item.ID)
                        .SendServer();
                }
                else
                {
                    CPacketMenuResponse(
                        menu->Serial, menu->ID, item.ID, item.Graphic, item.Color)
                        .SendServer();
                }

                menu->Replayed = true;
                menu->ReplyID = item.ID;
            }
            else
            {
                ::g_GumpManager.SelectMenu(menu->Serial, menu->ID, item.ID);
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
            if (menu->GrayMenu)
            {
                CPacketGrayMenuResponse(menu->Serial, menu->ID, item.ID).SendServer();
            }
            else
            {
                CPacketMenuResponse(
                    menu->Serial, menu->ID, item.ID, item.Graphic, item.Color)
                    .SendServer();
            }

            menu->Replayed = true;
            menu->ReplyID = item.ID;
        }
        else
        {
            ::g_GumpManager.SelectMenu(menu->Serial, menu->ID, item.ID);
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
    string text = menu->GrayMenu ? "Gray menu information:\nName: " : "Menu information:\nName: ";
    text += menu->Name;

    if (menu->Replayed)
    {
        text += "\nMenu replayed with id: " + std::to_string(menu->ReplyID);
    }

    for (const CMenuItem &item : menu->m_Items)
    {
        char buf[256] = { 0 };
        snprintf(
            buf,
            sizeof(buf),
            "\nItem: ID=%i Graphic=0x%04X Color=0x%04X Name=%s",
            item.ID,
            item.Graphic,
            item.Color,
            item.Name.c_str());
        text += buf;
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

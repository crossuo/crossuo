// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/menumanager.h), trimmed
// (lists are std::vector, strings are astr_t).

#pragma once

#include "core/global.h"
#include "model/commonitems/menu.h"

namespace xuoassist {

class CMenuHook
{
    astr_t m_Name;
    astr_t m_ItemName;

public:
    CMenuHook(const astr_t &name, const astr_t &itemName)
        : m_Name(name)
        , m_ItemName(itemName)
    {
    }
    ~CMenuHook() {}

    astr_t GetName() const { return m_Name; }
    astr_t GetItemName() const { return m_ItemName; }
};

class CMenuManager
{
private:
    CMenu *m_LastMenu{ nullptr };

    vector<CMenuHook> m_Hooks;

    vector<CMenu *> m_Items;

    void RemoveLastMenu();

public:
    CMenuManager();
    virtual ~CMenuManager();

    int GetMenuCount();

    CMenu *GetLastMenu();

    CMenu *GetMenu(const astr_t &name);

    void ClearMenus();

    void SetHook(const astr_t &name, const astr_t &itemName);

    void ClearHooks();

    // fed from the client's parsed 0x7C values (serial, id, name, gray-menu
    // flag, the entries); bool = pass-through, parity with the original
    bool OnMenuReceived(
        uint32_t serial, uint16_t id, const astr_t &name, bool grayMenu, const vector<CMenuItem> &items);

    // the client's 0x7D send sites report the user's menu choice here (was the
    // 0x7D MenuChoice send tap in the assistant's packetmanager)
    void OnMenuChoiceSent(uint32_t serial, uint16_t id, uint16_t index);

    bool SendChoiceIfMenuExists(const astr_t &name, const astr_t &itemName, const bool &sendPacket);

    bool SendMenuChoice(CMenu *menu, const astr_t &itemName, const bool &sendPacket);

    void InfoMenu(const astr_t &menuIndex);
};

extern CMenuManager g_MenuManager;

} // namespace xuoassist

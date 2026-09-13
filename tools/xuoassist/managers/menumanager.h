// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/menumanager.h), Qt-diet
// (QList -> std::vector, QString -> std::string).

#pragma once

#include "core/global.h"
#include "model/commonitems/menu.h"
#include "model/datareader.h"

namespace xuoassist {

class CMenuHook
{
    string m_Name;
    string m_ItemName;

public:
    CMenuHook(const string &name, const string &itemName)
        : m_Name(name)
        , m_ItemName(itemName)
    {
    }
    ~CMenuHook() {}

    string GetName() const { return m_Name; }
    string GetItemName() const { return m_ItemName; }
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

    CMenu *GetMenu(const string &name);

    void ClearMenus();

    void SetHook(const string &name, const string &itemName);

    void ClearHooks();

    bool OnPacketReceived(CDataReader &reader);

    bool OnPacketSend(CDataReader &reader);

    bool SendChoiceIfMenuExists(const string &name, const string &itemName, const bool &sendPacket);

    bool SendMenuChoice(CMenu *menu, const string &itemName, const bool &sendPacket);

    void InfoMenu(const string &menuIndex);
};

extern CMenuManager g_MenuManager;

} // namespace xuoassist


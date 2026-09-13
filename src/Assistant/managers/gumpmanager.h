// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/gumpmanager.h), trimmed
// (lists are std::vector, strings are std::string).

#pragma once

#include "core/global.h"
#include "model/commonitems/gump.h"
#include "model/commonitems/gumphook.h"
#include "model/datareader.h"

namespace xuoassist {

class CGumpManager
{
private:
    CGump *m_LastGump{ nullptr };

    vector<CGumpHook> m_Hooks;

    vector<CGump *> m_Items;

    void RemoveLastGump();

public:
    CGumpManager();
    virtual ~CGumpManager();

    int GetGumpCount();

    CGump *GetLastGump();

    CGump *GetGump(const uint &index);

    CGump *GetGump(const uint &serial, const uint &id);

    void ClearGumps();

    void AddHook(CGumpHook *hook);

    void ClearHooks();

    bool OnPacketReceived(CDataReader &reader);

    bool OnPacketSend(CDataReader &reader);

    bool SendGumpChoice(CGump *gump, CGumpHook &hook);

    void InfoGump(const string &gumpIndex);
};

extern CGumpManager g_GumpManager;

} // namespace xuoassist


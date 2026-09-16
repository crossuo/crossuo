// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/gumpmanager.h), trimmed
// (lists are std::vector, strings are astr_t).

#pragma once

#include "core/global.h"
#include "model/commonitems/gump.h"
#include "model/commonitems/gumphook.h"

namespace xuoassist {

class CGumpManager
{
private:
    vector<CGumpHook> m_Hooks;
    CGump *m_LastGump{ nullptr };
    vector<CGump *> m_Items;

public:
    CGumpManager();
    virtual ~CGumpManager();

    CGumpManager(const CGumpManager &) = delete;
    CGumpManager &operator=(const CGumpManager &) = delete;

    int GetGumpCount();
    CGump *GetLastGump();
    CGump *GetGump(const uint32_t &index);
    CGump *GetGump(const uint32_t &serial, const uint32_t &id);
    void ClearGumps();
    void AddHook(CGumpHook *hook);
    void ClearHooks();
    void AddGump(uint32_t serial, int x, int y, uint32_t id);
    void SetLastGumpType(const astr_t &cmd, const astr_t &line);
    void SetLastGumpText(const wstr_t &text);
    void RemoveLastGump();
    bool Consume();

    // the client's 0xB1 send sites report the user's gump choice here (was the
    // 0xB1 ChoiceGump send tap in the assistant's packetmanager); the id is
    // the value written to the wire (the master gump id when one is set)
    void OnGumpChoiceSent(uint32_t serial, uint32_t id, uint32_t button);
    bool SendGumpChoice(CGump *gump, CGumpHook &hook);
    void InfoGump(const astr_t &gumpIndex);
};

extern CGumpManager g_GumpManager;

} // namespace xuoassist

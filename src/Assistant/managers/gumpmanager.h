// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/gumpmanager.h), trimmed
// (lists are std::vector, strings are astr_t).

#pragma once

#include "core/global.h"
#include "model/commonitems/gumpitem.h"
#include "model/commonitems/gumphook.h"

namespace xuoassist {

// the assistant gump record: the parsed 0xB1 gump layout (item command lines
// and text lines) plus the replay state; the script Gump wrappers hold deep
// copies of it
struct GumpData
{
    uint32_t Serial = 0;
    uint32_t ID = 0;
    int X = 0;
    int Y = 0;
    bool Replayed = false;
    uint32_t ReplyID = 0;
    bool NoMove = false;
    bool NoClose = false;
    bool NoDispose = false;

    GUMP_LIST m_Items;
    vector<astr_t> m_Text;
};

class CGumpManager
{
private:
    vector<CGumpHook> m_Hooks;
    GumpData *m_LastGump{ nullptr };
    vector<GumpData *> m_Items;

public:
    CGumpManager();
    virtual ~CGumpManager();

    CGumpManager(const CGumpManager &) = delete;
    CGumpManager &operator=(const CGumpManager &) = delete;

    int GetGumpCount();
    GumpData *GetLastGump();
    GumpData *GetGump(const uint32_t &index);
    GumpData *GetGump(const uint32_t &serial, const uint32_t &id);
    void ClearGumps();
    void AddHook(CGumpHook *hook);
    void ClearHooks();
    void AddGump(uint32_t serial, int x, int y, uint32_t id);
    void SetLastGumpType(const astr_t &cmd, const astr_t &line);
    void SetLastGumpText(const wstr_t &text);
    void RemoveLastGump();
    bool Consume();

    // the client's 0xB1 send sites report the user's gump choice here; the id
    // is the value written to the wire (the master gump id when one is set)
    // the value written to the wire (the master gump id when one is set)
    void OnGumpChoiceSent(uint32_t serial, uint32_t id, uint32_t button);
    bool SendGumpChoice(GumpData *gump, CGumpHook &hook);
    void InfoGump(const astr_t &gumpIndex);
};

extern CGumpManager g_GumpManager;

} // namespace xuoassist

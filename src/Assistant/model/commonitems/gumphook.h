// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/gumphook.h): lists are std::vector,
// strings are astr_t.

#pragma once

#include "core/global.h"

namespace xuoassist {

class CGumpHookCheck
{
public:
    int Index = 0;
    bool State = false;

    CGumpHookCheck(const int &index, const bool &state)
        : Index(index)
        , State(state)
    {
    }
    ~CGumpHookCheck() {}
};

class CGumpHookEntry
{
public:
    int Index = 0;
    astr_t Text = "";

    CGumpHookEntry(const int &index, const astr_t &text)
        : Index(index)
        , Text(text)
    {
    }
    ~CGumpHookEntry() {}
};

class CGumpHook
{
public:
    int Index = 0;

    CGumpHook() {}
    CGumpHook(CGumpHook *original);
    ~CGumpHook() {}

    vector<CGumpHookEntry> m_Entries;
    vector<CGumpHookCheck> m_Checks;
};

} // namespace xuoassist

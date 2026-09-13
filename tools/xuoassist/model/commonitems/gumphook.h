// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/gumphook.h): QList -> std::vector,
// QString -> std::string.

#pragma once

#include "core/global.h"

namespace xuoassist {

class CGumpHookCheck
{
    SETGET(int, Index, 0)
    SETGET(bool, State, false)

public:
    CGumpHookCheck(const int &index, const bool &state)
        : m_Index(index)
        , m_State(state)
    {
    }
    ~CGumpHookCheck() {}
};

class CGumpHookEntry
{
    SETGET(int, Index, 0)
    SETGET(string, Text, "")

public:
    CGumpHookEntry(const int &index, const string &text)
        : m_Index(index)
        , m_Text(text)
    {
    }
    ~CGumpHookEntry() {}
};

class CGumpHook
{
    SETGET(int, Index, 0)

public:
    CGumpHook() {}
    CGumpHook(CGumpHook *original);
    ~CGumpHook() {}

    vector<CGumpHookEntry> m_Entries;
    vector<CGumpHookCheck> m_Checks;
};

} // namespace xuoassist


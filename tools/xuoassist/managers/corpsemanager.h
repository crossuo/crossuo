// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/corpsemanager.h), trimmed
// (lists are std::vector).

#pragma once

#include "core/global.h"

namespace xuoassist {

class CCorpseInfo
{
    SETGET(uint, Serial, 0)
    SETGET(int, Timer, 0)

public:
    CCorpseInfo() {}
    CCorpseInfo(const uint &serial, const int &timer)
        : m_Serial(serial)
        , m_Timer(timer)
    {
    }
    ~CCorpseInfo() {}
};

typedef vector<CCorpseInfo> CORPSE_INFO_LIST;

class CCorpseManager
{
private:
    CORPSE_INFO_LIST m_List;

public:
    CCorpseManager();
    virtual ~CCorpseManager();

    void Add(const CCorpseInfo &info);

    void Clear();

    // Tick method (was a 10 ms timer, now wired to the frame tick).
    void CheckCorpses();

    bool Empty() { return m_List.empty(); }
};

extern CCorpseManager g_CorpseManager;

} // namespace xuoassist


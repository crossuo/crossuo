// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/spellmanager.h), trimmed
// (maps are std::map, astr_t lists are vector<vector<astr_t>>).

#pragma once

#include <map>

#include "core/global.h"

namespace xuoassist {

typedef std::map<astr_t, int> SPELLS_MAP;

class CSpellManager
{
private:
    SPELLS_MAP m_Spells;

public:
    CSpellManager();
    ~CSpellManager();

    CSpellManager(const CSpellManager &) = delete;
    CSpellManager &operator=(const CSpellManager &) = delete;

    vector<vector<astr_t>> m_SpellbookSpellNames;

    void Insert(const astr_t &name, const int &index);

    int Find(const astr_t& name) const;

    void Cast(const astr_t &name);

    // the raw-index send (apiscript's numeric Cast/UseSkill form): the
    // client call site inside a recording suspension so assistant-initiated
    // uses do not record themselves
    void CastIndex(int index);

    void TargetCast(const astr_t &name, const uint32_t &serial) const;
};

extern CSpellManager g_SpellManager;

} // namespace xuoassist

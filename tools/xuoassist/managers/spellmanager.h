// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/spellmanager.h), Qt-diet
// (QMap -> std::map, QList<QStringList> -> vector<vector<string>>).

#pragma once

#include <map>

#include "core/global.h"

namespace xuoassist {

typedef std::map<string, int> SPELLS_MAP;

class CSpellManager
{
private:
    SPELLS_MAP m_Spells;

public:
    CSpellManager();
    ~CSpellManager();

    vector<vector<string>> m_SpellbookSpellNames;

    void Insert(const string &name, const int &index);

    int Find(string name);

    void Cast(const string &name);

    void TargetCast(const string &name, const uint &serial);
};

extern CSpellManager g_SpellManager;

} // namespace xuoassist


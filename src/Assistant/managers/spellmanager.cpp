// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein


#include "spellmanager.h"
#include "../core/assist.h"
#include "../core/packets.h"

namespace xuoassist {

static string to_lower_copy(string text)
{
    for (char &c : text)
    {
        c = (char)tolower(c);
    }
    return text;
}

CSpellManager g_SpellManager;

CSpellManager::CSpellManager() {}

CSpellManager::~CSpellManager() {}

void CSpellManager::Insert(const string &name, const int &index)
{
    m_Spells.insert(std::make_pair(name, index));
    m_Spells.insert(std::make_pair(std::to_string(index), index));
}

int CSpellManager::Find(string name)
{
    if (name == "last" || name == "lastspell")
    {
        return g_LastSpellIndex;
    }

    SPELLS_MAP::iterator it = m_Spells.find(name);

    if (it != m_Spells.end())
    {
        return it->second;
    }

    return -1;
}

void CSpellManager::Cast(const string &name)
{
    int index = Find(to_lower_copy(name));

    if (index != -1)
    {
        CPacketCastSpellRequest(index).SendClient();
    }
    else
    {
        g_Assistant.ClientPrint("Spell '" + name + "' is not found.");
    }
}

void CSpellManager::TargetCast(const string &name, const uint &serial)
{
    int index = Find(to_lower_copy(name));

    if (index != -1)
    {
        CPacketTargetCastSpell(index, serial).SendServer();
    }
    else
    {
        g_Assistant.ClientPrint("Spell '" + name + "' is not found.");
    }
}

} // namespace xuoassist

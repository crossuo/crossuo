// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein


#include "spellmanager.h"
#include "../core/assist.h"
#include "../macros/macroexecutor.h" // RecordingSuspension (assistant-originated casts do not record)
#include "CrossUO.h" // ::g_Game (the client send bridge)
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
        CastIndex(index);
    }
    else
    {
        g_Assistant.ClientPrint("Spell '" + name + "' is not found.");
    }
}

void CSpellManager::CastIndex(int index)
{
    // assistant-initiated casts must not record themselves; the client's
    // CGame::CastSpell is the recording tap (was: the 0xFC CastSpellRequest
    // injection, whose client handler performed the same send)
    RecordingSuspension suspension;

    g_Game.CastSpell(index);
}

void CSpellManager::TargetCast(const string &name, const uint &serial)
{
    int index = Find(to_lower_copy(name));

    if (index != -1)
    {
        // the targeted 0xBF 0x002D send has no client call site (the client's
        // builders target spellbooks, not objects) - the builder stays, and
        // the direct wire send is wrapped like the client-call paths above
        RecordingSuspension suspension;

        CPacketTargetCastSpell(index, serial).SendServer();
    }
    else
    {
        g_Assistant.ClientPrint("Spell '" + name + "' is not found.");
    }
}

} // namespace xuoassist

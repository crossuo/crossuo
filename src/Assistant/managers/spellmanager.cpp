// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein


#include "spellmanager.h"
#include "../core/assist.h"
#include "../macros/macroexecutor.h" // RecordingSuspension (assistant-originated casts do not record)
#include "CrossUO.h" // ::g_Game (the client send bridge)
#include "Network/Packets.h" // CPacketTargetCastSpell (the targeted 0xBF builder)

namespace xuoassist {

CSpellManager g_SpellManager;
CSpellManager::CSpellManager() {}
CSpellManager::~CSpellManager() {}

void CSpellManager::Insert(const astr_t &name, const int &index)
{
    m_Spells.insert(std::make_pair(name, index));
    m_Spells.insert(std::make_pair(str_from(index), index));
}

int CSpellManager::Find(astr_t name)
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

void CSpellManager::Cast(const astr_t &name)
{
    int index = Find(str_lower(name));

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

void CSpellManager::TargetCast(const astr_t &name, const uint &serial)
{
    int index = Find(str_lower(name));

    if (index != -1)
    {
        // the targeted 0xBF 0x002D send has no client call site (the client's
        // builders target spellbooks, not objects); the wire send runs no
        // recording taps, so no suspension is needed
        ::CPacketTargetCastSpell(index, serial).Send();
    }
    else
    {
        g_Assistant.ClientPrint("Spell '" + name + "' is not found.");
    }
}

} // namespace xuoassist

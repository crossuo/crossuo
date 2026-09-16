// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/skillmanager.cpp).

#include "skillmanager.h"
#include "../core/assist.h"
#include "../macros/macroexecutor.h" // RecordingSuspension (assistant-originated uses do not record)
#include "Network/Packets.h" // CPacketTargetUseSkill (the targeted 0xBF builder)
#include "CrossUO.h" // ::g_Game (the client send bridge)
#include "Managers/SkillsManager.h" // ::g_SkillsManager

namespace xuoassist {

static astr_t to_lower_copy(astr_t text)
{
    for (char &c : text)
    {
        c = (char)tolower(c);
    }
    return text;
}

CSkillManager g_SkillManager;

CSkillManager::CSkillManager() {}

CSkillManager::~CSkillManager() {}

void CSkillManager::Insert(const astr_t &name, const bool &usable, const int &index)
{
    (void)usable;

    m_AllSkills.insert(std::make_pair(name, index));
    m_AllSkills.insert(std::make_pair(str_from(index), index));
}

int CSkillManager::GetIndex(const astr_t &name)
{
    SKILLS_MAP::iterator it = m_AllSkills.find(to_lower_copy(name));

    if (it != m_AllSkills.end())
    {
        return it->second;
    }

    return -1;
}

int CSkillManager::Find(const astr_t &name)
{
    // the client lookup handles the "last" aliases, exact names and partial
    // names (button skills only)
    return g_SkillsManager.Find(name);
}

void CSkillManager::Use(const astr_t &name)
{
    int index = Find(to_lower_copy(name));

    if (index != -1)
    {
        UseIndex(index);
    }
    else
    {
        g_Assistant.ClientPrint("Skill '" + name + "' is not found.");
    }
}

void CSkillManager::UseIndex(int index)
{
    // assistant-initiated skill uses must not record themselves; the client's
    // CGame::UseSkill is the recording tap (was: the 0xFC UseSkillRequest
    // injection, whose client handler performed the same send)
    RecordingSuspension suspension;

    g_Game.UseSkill(index);
}

void CSkillManager::TargetUse(const astr_t &name, const uint &serial)
{
    int index = Find(to_lower_copy(name));

    if (index != -1)
    {
        // the targeted 0xBF 0x002E send has no client call site (the client's
        // builders target spellbooks, not objects); the wire send runs no
        // recording taps, so no suspension is needed
        ::CPacketTargetUseSkill(index + 1, serial).Send();
    }
    else
    {
        g_Assistant.ClientPrint("Skill '" + name + "' is not found.");
    }
}

} // namespace xuoassist

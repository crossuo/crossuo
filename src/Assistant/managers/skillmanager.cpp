// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/skillmanager.cpp).

#include "skillmanager.h"
#include "../core/assist.h"
#include "../core/packets.h" // CPacketTargetUseSkill (the targeted 0xBF builder)
#include "../macros/macroexecutor.h" // RecordingSuspension (assistant-originated uses do not record)
#include "CrossUO.h" // ::g_Game (the client send bridge)
#include "Managers/SkillsManager.h" // ::g_SkillsManager

namespace xuoassist {

static string to_lower_copy(string text)
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

void CSkillManager::Insert(const string &name, const bool &usable, const int &index)
{
    (void)usable;

    m_AllSkills.insert(std::make_pair(name, index));
    m_AllSkills.insert(std::make_pair(std::to_string(index), index));
}

int CSkillManager::GetIndex(const string &name)
{
    SKILLS_MAP::iterator it = m_AllSkills.find(to_lower_copy(name));

    if (it != m_AllSkills.end())
    {
        return it->second;
    }

    return -1;
}

int CSkillManager::Find(const string &name)
{
    // the client lookup handles the "last" aliases, exact names and partial
    // names (button skills only)
    return g_SkillsManager.Find(name);
}

void CSkillManager::Use(const string &name)
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

void CSkillManager::TargetUse(const string &name, const uint &serial)
{
    int index = Find(to_lower_copy(name));

    if (index != -1)
    {
        // the targeted 0xBF 0x002E send has no client call site (the client's
        // builders target spellbooks, not objects) - the builder stays, and
        // the direct wire send is wrapped like the client-call paths above
        RecordingSuspension suspension;

        CPacketTargetUseSkill(index + 1, serial).SendServer();
    }
    else
    {
        g_Assistant.ClientPrint("Skill '" + name + "' is not found.");
    }
}

} // namespace xuoassist

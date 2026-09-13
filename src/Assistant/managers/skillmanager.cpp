// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/skillmanager.cpp).

#include "skillmanager.h"
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

CSkillManager g_SkillManager;

CSkillManager::CSkillManager() {}

CSkillManager::~CSkillManager() {}

void CSkillManager::Insert(const string &name, const bool &usable, const int &index)
{
    m_AllSkills.insert(std::make_pair(name, index));
    m_AllSkills.insert(std::make_pair(std::to_string(index), index));

    if (usable)
    {
        m_Skills.insert(std::make_pair(name, index));
        m_Skills.insert(std::make_pair(std::to_string(index), index));
    }
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
    if (name == "last" || name == "lastskill")
    {
        return g_LastSkillIndex;
    }

    SKILLS_MAP::iterator it = m_Skills.find(name);

    if (it != m_Skills.end())
    {
        return it->second;
    }

    return -1;
}

void CSkillManager::Use(const string &name)
{
    int index = Find(to_lower_copy(name));

    if (index != -1)
    {
        CPacketUseSkillRequest(index).SendClient();
    }
    else
    {
        g_Assistant.ClientPrint("Skill '" + name + "' is not found.");
    }
}

void CSkillManager::TargetUse(const string &name, const uint &serial)
{
    int index = Find(to_lower_copy(name));

    if (index != -1)
    {
        CPacketTargetUseSkill(index + 1, serial).SendServer();
    }
    else
    {
        g_Assistant.ClientPrint("Skill '" + name + "' is not found.");
    }
}

} // namespace xuoassist

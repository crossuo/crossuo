// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2017 Hotride

#include <algorithm>
#include <cstdint>
#include "SkillsManager.h"
#include "../Globals.h"
#include "../CrossUO.h" // ::g_Game (the client send bridge)
#if defined(XUO_ASSISTANT)
#include <core/assist.h>          // xuoassist::g_Assistant (the not-found error print path)
#include <macros/macroexecutor.h> // xuoassist::RecordingSuspension (assistant-originated uses do not record)
#endif
#include <common/logging/logging.h>
#include <common/str.h>
#include <xuocore/uodata.h>

CSkillsManager g_SkillsManager;

CSkill::CSkill(bool haveButton, const astr_t& name)
    : Button(haveButton)
{
    if (name.length() != 0u)
    {
        Name = name;
    }
    else
    {
        Name = "NoNameSkill";
    }
}

bool CSkillsManager::Init()
{
    if (Count != 0)
    {
        return false;
    }

    for (const auto &skill : g_FileManager.m_Skills)
    {
        Info(Client, "%d Skill: %s", Count, skill.Name.c_str());
        Add(CSkill(skill.Iteractive, skill.Name));
    }

    Info(Client, "skills count: %d", Count);
    Info(Client, "sorting skills");
    Sort();
    return true;
}

void CSkillsManager::Add(const CSkill &skill)
{
    m_Skills.push_back(skill);
    Count++;
}

void CSkillsManager::Clear()
{
    Count = 0;
    SkillsTotal = 0.0f;
    m_Skills.clear();
    m_SortedTable.clear();
}

CSkill *CSkillsManager::Get(uint32_t index)
{
    if (index < Count)
    {
        return &m_Skills[index];
    }
    return nullptr;
}

int CSkillsManager::Find(const astr_t &name)
{
    if (name == "last" || name == "lastskill")
    {
        return g_LastSkillIndex;
    }

    const astr_t query = str_lower(name);

    // An empty query would match every name (std::string::find("")),
    // so it addresses nothing
    if (query.empty())
    {
        return -1;
    }

    // An exact (case-insensitive) name match wins over an earlier
    // partial match
    for (uint32_t i = 0; i < Count; i++)
    {
        if (m_Skills[i].Button && str_lower(m_Skills[i].Name) == query)
        {
            return (int)i;
        }
    }

    // Only skills with a use button are addressable by name; the first
    // partial match is the fallback
    for (uint32_t i = 0; i < Count; i++)
    {
        if (m_Skills[i].Button && str_lower(m_Skills[i].Name).find(query) != astr_t::npos)
        {
            return (int)i;
        }
    }

    return -1;
}

// The shared name-based use entry (the former xuoassist CSkillManager::Use):
// the lookup, the not-found error print, then the send through
// CGame::UseSkill, which is the macro recording tap. The recording
// suspension keeps assistant-initiated uses from recording themselves;
// client-path uses (hotkeys, gump buttons) reach UseSkill directly and
// still record.
void CSkillsManager::Use(const astr_t &name)
{
    const int index = Find(name);
    if (index == -1)
    {
#if defined(XUO_ASSISTANT)
        xuoassist::g_Assistant.ClientPrint("Skill '" + name + "' is not found.");
#endif
        return;
    }

#if defined(XUO_ASSISTANT)
    // assistant-initiated skill uses must not record themselves; the
    // client's CGame::UseSkill is the recording tap
    xuoassist::RecordingSuspension suspension;
#endif
    g_Game.UseSkill(index);
}

bool CSkillsManager::CompareName(const astr_t &str1, const astr_t &str2)
{
    // Compute the minimum string length to compare
    const auto len = (int)std::min(str1.length(), str2.length());

    bool result = false;

    // Walk every character of the string, comparing them pairwise
    for (int i = 0; i < len; i++)
    {
        char c1 = str1.at(i);
        char c2 = str2.at(i);

        if (c1 < c2)
        {
            return true;
        }
        if (c1 > c2)
        {
            return false;
        }
    }

    // Return the result
    return result;
}

void CSkillsManager::Sort()
{
    m_SortedTable.resize(Count, 0xFF);
    std::vector<uint8_t> bufTable(Count, 0xFF);

    // Set the first element to zero and the processed-skill count to 1
    int parsed = 1;
    bufTable[0] = 0;

    // Walk all skills (except the first)
    for (uint32_t i = 1; i < Count; i++)
    {
        // Walk the processed entries
        for (int j = 0; j < parsed; j++)
        {
            // If it can be inserted at the current position:
            if (CompareName(m_Skills[bufTable[j]].Name, m_Skills[i].Name))
            {
                // Remember the skill index
                uint8_t buf = bufTable[j];
                // Overwrite it
                bufTable[j] = (uint8_t)i;

                // Move to the next skill
                j++;

                // Walk the remaining processed entries, shifting indexes as needed
                for (; j < parsed; j++)
                {
                    uint8_t ptr = bufTable[j];
                    bufTable[j] = buf;
                    buf = ptr;
                }

                // Write the index into the current processed entry
                bufTable[parsed] = buf;

                // Increment the counter
                parsed++;

                break;
            }
        }
    }

    for (int i = 0, j = parsed - 1; i < parsed; i++, j--)
    {
        m_SortedTable[i] = bufTable[j];
    }
}

int CSkillsManager::GetSortedIndex(uint32_t index)
{
    if (index < Count)
    {
        return m_SortedTable[index];
    }
    return -1;
}

void CSkillsManager::UpdateSkillsSum()
{
    SkillsTotal = 0.0f;
    for (const CSkill &skill : m_Skills)
    {
        SkillsTotal += skill.Value;
    }
}

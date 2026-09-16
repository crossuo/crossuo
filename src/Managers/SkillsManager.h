// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2017 Hotride

#pragma once

#include <stdint.h>
#include <vector>
#include <common/str.h>

struct CSkill
{
    bool Button = 0;
    astr_t Name;
    float BaseValue = 0.0f;
    float Value = 0.0f;
    // Value at login or at the last delta reset; deltas are measured against it
    float OldValue = 0.0f;
    float Cap = 0.0f;
    uint8_t LockStatus = 0; // skill lock: 0 = up, 1 = down, 2 = locked

    CSkill() = default;
    CSkill(bool haveButton, astr_t name);
    ~CSkill() = default;
};

class CSkillsManager
{
public:
    uint32_t Count = 0;
    bool SkillsRequested = false;
    float SkillsTotal = 0.0f;

private:
    std::vector<CSkill> m_Skills;
    std::vector<uint8_t> m_SortedTable;

    bool CompareName(const astr_t &str1, const astr_t &str2);

public:
    CSkillsManager() = default;
    ~CSkillsManager() = default;

    bool Init();
    void Add(const CSkill &skill);
    void Clear();
    CSkill *Get(uint32_t index);
    // Case-insensitive partial name match; "last"/"lastskill" resolves to the
    // last used skill. Returns the skill index or -1.
    int Find(const astr_t &name);
    // Name-based use: resolves via Find, prints the assistant not-found
    // error on a failed lookup, then sends through CGame::UseSkill (the
    // macro recording tap). Assistant builds suspend recording around the
    // send so assistant-initiated uses do not record themselves.
    void Use(const astr_t &name);
    void Sort();
    int GetSortedIndex(uint32_t index);
    void UpdateSkillsSum();
};

extern CSkillsManager g_SkillsManager;

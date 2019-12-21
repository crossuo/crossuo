// MIT License
// Copyright (C) Obtober 2017 Hotride

#pragma once

#include <stdint.h>
#include <vector>
#include <common/str.h>

class CSkill
{
public:
    bool Button = 0;
    astr_t Name = "";
    float BaseValue = 0.0f;
    float Value = 0.0f;
    float Cap = 0.0f;
    uint8_t Status = 0;

    CSkill() {}
    CSkill(bool haveButton, const astr_t &name);
    ~CSkill() {}
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
    CSkillsManager() {}
    ~CSkillsManager() {}

    bool Load();
    void Add(const CSkill &skill);
    void Clear();
    CSkill *Get(uint32_t index);
    void Sort();
    int GetSortedIndex(uint32_t index);
    void UpdateSkillsSum();
};

extern CSkillsManager g_SkillsManager;

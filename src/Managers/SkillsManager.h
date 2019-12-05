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
    std::string Name = "";
    float BaseValue = 0.0f;
    float Value = 0.0f;
    float Cap = 0.0f;
    uint8_t Status = 0;

    CSkill() {}
    CSkill(bool haveButton, const std::string &name);
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

    bool CompareName(const std::string &str1, const std::string &str2);

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

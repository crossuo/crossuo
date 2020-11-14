// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <common/fs.h>

class CSkillGroupObject;

class CSkillGroupManager
{
    int Count = 0;

private:
    void MakeDefaultMiscellaneous();
    void MakeDefaultCombat();
    void MakeDefaultTradeSkills();
    void MakeDefaultMagic();
    void MakeDefaultWilderness();
    void MakeDefaultThieving();
    void MakeDefaultBard();

public:
    CSkillGroupObject *m_Groups{ nullptr };

    CSkillGroupManager();
    ~CSkillGroupManager();

    void MakeDefault();
    void Clear();
    void Add(CSkillGroupObject *group);
    bool Remove(CSkillGroupObject *group);
    bool Load(const fs_path &path);
    void Save(const fs_path &path);
};

extern CSkillGroupManager g_SkillGroupManager;

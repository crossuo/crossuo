// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

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
    bool Load(const os_path &path);
    void Save(const os_path &path);
};

extern CSkillGroupManager g_SkillGroupManager;

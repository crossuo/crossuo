// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseQueue.h"

class CBaseProfession : public CBaseQueueItem
{
public:
    string Name = "";
    string TrueName = "";
    wstring Description = {};
    uint32_t NameClilocID = 0;
    uint32_t DescriptionClilocID = 0;
    PROFESSION_TYPE Type = PT_NO_PROF;
    uint16_t Gump = 0;
    bool TopLevel = false;
    int DescriptionIndex = 0;

    CBaseProfession();
    virtual ~CBaseProfession();

    bool AddDescription(int desc, const string &name, const char *val);
};

class CProfessionCategory : public CBaseProfession
{
public:
    string Childrens = "|";

    CProfessionCategory();
    virtual ~CProfessionCategory();

    void AddChildren(const string &child);
};

class CProfession : public CBaseProfession
{
public:
    uint8_t Str = 0;
    uint8_t Int = 0;
    uint8_t Dex = 0;

private:
    uint8_t m_SkillIndex[4];
    uint8_t m_SkillValue[4];

public:
    CProfession();
    virtual ~CProfession();

    void SetSkillIndex(int index, uint8_t val)
    {
        if (index >= 0 && index < 4)
            m_SkillIndex[index] = val;
    }
    void SetSkillValue(int index, uint8_t val)
    {
        if (index >= 0 && index < 4)
            m_SkillValue[index] = val;
    }

    uint8_t GetSkillIndex(int index) const
    {
        if (index >= 0 && index < 4)
            return m_SkillIndex[index];
        else
            return 0xFF;
    }
    uint8_t GetSkillValue(int index) const
    {
        if (index >= 0 && index < 4)
            return m_SkillValue[index];
        else
            return 0xFF;
    }
};

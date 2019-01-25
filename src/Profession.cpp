// MIT License
// Copyright (C) August 2016 Hotride

#include "Profession.h"
#include "Managers/FontsManager.h"

CBaseProfession::CBaseProfession()
{
}

CBaseProfession::~CBaseProfession()
{
}

bool CBaseProfession::AddDescription(int desc, const string &name, const char *val)
{
    DEBUG_TRACE_FUNCTION;

    bool result = (DescriptionIndex == desc);
    if (result)
    {
        if (NameClilocID == 0u)
        {
            Name = name;
        }

        g_FontManager.SetUseHTML(true);
        if (desc == -2)
        {
            Description = ToWString(val);
        }
        else
        {
            Description = ToWString(name + "\n" + val);
        }

        g_FontManager.SetUseHTML(false);
    }
    else
    {
        for (CBaseProfession *obj = (CBaseProfession *)m_Items; obj != nullptr && !result;
             obj = (CBaseProfession *)obj->m_Next)
        {
            result = obj->AddDescription(desc, name, val);
        }
    }

    return result;
}

CProfessionCategory::CProfessionCategory()
    : CBaseProfession()
{
}

CProfessionCategory::~CProfessionCategory()
{
}

void CProfessionCategory::AddChildren(const string &child)
{
    Childrens += child + "|";
}

CProfession::CProfession()
    : CBaseProfession()
{
    memset(&m_SkillIndex[0], 0, sizeof(m_SkillIndex));
    memset(&m_SkillValue[0], 0, sizeof(m_SkillValue));
}

CProfession::~CProfession()
{
}

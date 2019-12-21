// MIT License
// Copyright (C) August 2016 Hotride

#include <common/str.h>
#include "Profession.h"
#include "Managers/FontsManager.h"

CBaseProfession::CBaseProfession()
{
}

CBaseProfession::~CBaseProfession()
{
}

bool CBaseProfession::AddDescription(int desc, const astr_t &name, const char *val)
{
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
            Description = wstr_from(val);
        }
        else
        {
            Description = wstr_from(name + "\n" + val);
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

void CProfessionCategory::AddChildren(const astr_t &child)
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

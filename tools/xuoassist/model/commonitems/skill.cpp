// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/skill.cpp), verbatim.

#include "skill.h"

namespace xuoassist {

CSkill g_Skills[MAX_SKILLS_COUNT];
int g_SkillsCount = MAX_SKILLS_COUNT;

CSkill::CSkill(const bool &haveButton, const string &name)
    : m_Button(haveButton)
{
    if (name.length() != 0)
    {
        m_Name = name;
    }
    else
    {
        m_Name = "NoNameSkill";
    }
}

} // namespace xuoassist

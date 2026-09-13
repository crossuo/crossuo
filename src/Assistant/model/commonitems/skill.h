// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (commonitems/skill.h): strings are std::string.

#pragma once

#include "core/global.h"

namespace xuoassist {

class CSkill
{
    SETGET(bool, Button, 0)
    SETGET(string, Name, "")
    SETGET(float, BaseValue, 0.0f)
    SETGET(float, Value, 0.0f)
    SETGET(float, OldValue, 0.0f)
    SETGET(float, Cap, 0.0f)
    SETGET(uchar, Status, 0)

public:
    CSkill() {}
    CSkill(const bool &haveButton, const string &name);
    ~CSkill() {}
};

static const int MAX_SKILLS_COUNT = 60;
extern CSkill g_Skills[MAX_SKILLS_COUNT];
extern int g_SkillsCount;

} // namespace xuoassist


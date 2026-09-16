// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The assistant-owned name/number -> index lookup for the skill-cast text
// commands and hotkeys. The skill values/lock state themselves live in the
// client's skill table (g_SkillsManager); Use/TargetUse resolve through it.

#pragma once

#include <map>

#include "core/global.h"

namespace xuoassist {

typedef std::map<astr_t, int> SKILLS_MAP;

class CSkillManager
{
private:
    SKILLS_MAP m_AllSkills;

public:
    CSkillManager();
    ~CSkillManager();

    void Insert(const astr_t &name, const bool &usable, const int &index);

    int GetIndex(const astr_t &name);

    int Find(const astr_t &name);

    void Use(const astr_t &name);

    // the raw-index send (apiscript's numeric Cast/UseSkill form): the
    // client call site inside a recording suspension so assistant-initiated
    // uses do not record themselves
    void UseIndex(int index);

    void TargetUse(const astr_t &name, const uint &serial);
};

extern CSkillManager g_SkillManager;

} // namespace xuoassist

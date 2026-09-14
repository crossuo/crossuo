// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// The assistant-owned name/number -> index lookup for the skill-cast text
// commands and hotkeys. The skill values/lock state themselves live in the
// client's skill table (g_SkillsManager); Use/TargetUse resolve through it.

#pragma once

#include <map>

#include "core/global.h"

namespace xuoassist {

typedef std::map<string, int> SKILLS_MAP;

class CSkillManager
{
private:
    SKILLS_MAP m_AllSkills;

public:
    CSkillManager();
    ~CSkillManager();

    void Insert(const string &name, const bool &usable, const int &index);

    int GetIndex(const string &name);

    int Find(const string &name);

    void Use(const string &name);

    void TargetUse(const string &name, const uint &serial);
};

extern CSkillManager g_SkillManager;

} // namespace xuoassist

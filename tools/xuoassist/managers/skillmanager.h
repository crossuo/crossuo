// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (managers/skillmanager.h): the assistant-owned
// name->index lookup maps. The skill VALUES/lock state itself lives in
// g_Skills[] (model/commonitems/skill.h, ported in Phase 1a); the client's
// g_SkillsManager is not used for this lookup table.

#ifndef XUOASSIST_SKILLMANAGER_H
#define XUOASSIST_SKILLMANAGER_H

#include <map>

#include "core/global.h"

namespace xuoassist {

typedef std::map<string, int> SKILLS_MAP;

class CSkillManager
{
private:
    SKILLS_MAP m_AllSkills;
    SKILLS_MAP m_Skills;

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

#endif // XUOASSIST_SKILLMANAGER_H

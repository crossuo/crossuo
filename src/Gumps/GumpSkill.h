// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpSkill : public CGump
{
private:
    static constexpr int ID_GS_LOCK_MOVING = 1;
    static constexpr int ID_GS_BUTTON_REMOVE_FROM_GROUP = 2;

protected:
    virtual void CalculateGumpState() override;
    CGUIButton *m_SkillUnlocker = nullptr;
    CGUIText *m_ValueText = nullptr;
    float m_LastDrawnValue = -1.0f;

public:
    virtual void PrepareContent() override;

public:
    CGumpSkill(int serial, int x, int y);
    virtual ~CGumpSkill() override;

    // the gump's drawn box: skill gumps vary in height with the skill name
    // texture (and the width is the resizepic width), so the group snapping
    // math uses these instead of hardcoded constants
    int m_Width{ 140 };
    int m_Height{ 20 };

    CGumpSkill *m_GroupNext = nullptr;
    CGumpSkill *m_GroupPrev = nullptr;

    // set when the gump spawns as a live drag-out preview from the skills
    // list: the release that spawned it must not also use the skill
    bool SkipNextUse = false;

    CGumpSkill *GetTopSkill();
    CGumpSkill *GetNearSkill(int &x, int &y);
    bool GetSkillGroupOffset(int &x, int &y);
    void UpdateGroup(int x, int y);
    void AddSkill(CGumpSkill *skill);
    void RemoveFromGroup();
    bool InGroup() { return (m_GroupNext != nullptr || m_GroupPrev != nullptr); }

    virtual void InitToolTip() override;
    GUMP_BUTTON_EVENT_H override;

    virtual void OnLeftMouseButtonUp() override;
};

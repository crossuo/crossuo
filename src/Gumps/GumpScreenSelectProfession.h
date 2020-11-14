// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpScreenSelectProfession : public CGump
{
private:
    void UpdateContentOld();
    void UpdateContentNew();
    void NoFeatureError();

    void ShuffleStats(int id, int maxSum, int maxVal);
    void ShuffleSkills(int id);

    CGUISlider *m_StatsSliders[3];
    CGUISlider *m_SkillsSliders[4];

public:
    CGumpScreenSelectProfession();
    virtual ~CGumpScreenSelectProfession();

    virtual void UpdateContent() override;

    virtual void InitToolTip() override;

    GUMP_BUTTON_EVENT_H override;
    GUMP_TEXT_ENTRY_EVENT_H override { OnButton(serial); }
    GUMP_SLIDER_CLICK_EVENT_H override;
    GUMP_SLIDER_MOVE_EVENT_H override;
};

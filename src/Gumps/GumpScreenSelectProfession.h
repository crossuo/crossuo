// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Gump.h"
#include "Gump1998ScrollChoice.h"

class CGUITextEntry;
class CGUIGumppic;
class CGUIText;
class CEntryText;

class CGumpScreenSelectProfession : public CGump
{
private:
    void UpdateContentOld();
    void UpdateContentNew();

    //1.25.32 (1998) CharStat step: name/password/stats/profession picker
    void UpdateContent1998();

    void NoFeatureError();

    void ShuffleStats(int id, int maxSum, int maxVal);
    void ShuffleSkills(int id);

    CGUISlider *m_StatsSliders[3];
    CGUISlider *m_SkillsSliders[4];

public:
    //the real password store; the visible entry keeps asterisk placeholders
    //only (same masking scheme as the login screen)
    CEntryText *m_Password{ nullptr };

    //1.25.32 CharStat widgets (recreated by UpdateContent1998); the stage
    //routes text input into these entries
    CGUIGumppic *m_1998NameArt{ nullptr };
    CGUIGumppic *m_1998PasswordArt{ nullptr };
    CGUITextEntry *m_1998NameEntry{ nullptr };
    CGUITextEntry *m_1998PasswordFake{ nullptr };

protected:
    CGUISlider *m_1998StatSliders[3];
    CGUIText *m_1998StatValue[3];
    CGUIText *m_1998StatLeft[3];
    bool m_1998ProfessionPopupOpen = false;
    C1998ScrollChoice m_1998ProfessionPopup;

    //1.25.32 (1998) InitSkill step (page 2): the three skill picks
    C1998ScrollChoice m_1998SkillPopup;
    bool m_1998SkillPopupOpen = false;
    int m_1998PickSlot = 0;
    bool m_1998MsgDialog = false;
    astr_t m_1998MsgText;
    CGUIText *m_1998SkillName[3];
    CGUISlider *m_1998SkillSliders[3];
    CGUIText *m_1998SkillValue[3];

public:
    CGumpScreenSelectProfession();
    virtual ~CGumpScreenSelectProfession() override;

    virtual void UpdateContent() override;
    virtual void PrepareContent() override;

    virtual void InitToolTip() override;

    GUMP_BUTTON_EVENT_H override;
    GUMP_TEXT_ENTRY_EVENT_H override { OnButton(serial); }
    GUMP_SLIDER_CLICK_EVENT_H override;
    GUMP_SLIDER_MOVE_EVENT_H override;
};

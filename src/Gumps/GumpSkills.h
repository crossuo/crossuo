// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GumpBaseScroll.h"
#include "../Platform.h"

class CGumpSkills : public CGumpBaseScroll
{
private:
    bool m_ShowReal = false;
    bool m_ShowCap = false;

    class CSkillGroupObject *GetGroupUnderCursor(int &index);

    CGUIGumppic *m_BottomLine{ nullptr };
    CGUIGumppic *m_Comment{ nullptr };
    CGUIButton *m_CreateGroup{ nullptr };
    CGUIText *m_SkillSum{ nullptr };
    CGUICheckbox *m_CheckboxShowReal{ nullptr };
    CGUICheckbox *m_CheckboxShowCap{ nullptr };

    void UpdateGroupPositions();
    void SetGroupTextFromEntry();

protected:
    virtual void CalculateGumpState() override;

    virtual void UpdateHeight() override;

public:
    CGumpSkills(short x, short y, bool minimized, int height);
    virtual ~CGumpSkills();

    void UpdateGroupText();

    CGUISkillGroup *GetSkillGroup(int index);
    CGUISkillItem *GetSkill(int index);

    void UpdateSkillValue(int index);
    void UpdateSkillValues();
    void UpdateSkillsSum();
    virtual void Init();

    virtual void InitToolTip() override;
    virtual void PrepareContent() override;

    GUMP_BUTTON_EVENT_H override;
    GUMP_CHECKBOX_EVENT_H override;
    GUMP_TEXT_ENTRY_EVENT_H override;

    virtual void OnLeftMouseButtonUp() override;
    virtual bool OnLeftMouseButtonDoubleClick() override;
    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;
};

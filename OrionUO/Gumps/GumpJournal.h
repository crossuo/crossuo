// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GumpBaseScroll.h"

class CGumpJournal : public CGumpBaseScroll
{
private:
    CGUIGumppic *m_BottomLine{ nullptr };
    CGUIButton *m_TextLocker{ nullptr };
    CGUICheckbox *m_CheckboxShowSystem{ nullptr };
    CGUICheckbox *m_CheckboxShowObjects{ nullptr };
    CGUICheckbox *m_CheckboxShowClient{ nullptr };
    CGUIText *m_TextShowSystem{ nullptr };
    CGUIText *m_TextShowObjects{ nullptr };
    CGUIText *m_TextShowClient{ nullptr };

protected:
    virtual void UpdateHeight() override;
    int RecalculateHeight();

public:
    CGumpJournal(short x, short y, bool minimized, int height);
    virtual ~CGumpJournal();

    void AddText(class CTextData *obj);
    void DeleteText(class CTextData *obj);

    virtual void InitToolTip() override;

    GUMP_BUTTON_EVENT_H;
    GUMP_CHECKBOX_EVENT_H;
    GUMP_SLIDER_CLICK_EVENT_H { m_TextLocker->Visible = true; }
    GUMP_SLIDER_MOVE_EVENT_H { m_TextLocker->Visible = true; }
    GUMP_SCROLL_BUTTON_EVENT_H { m_TextLocker->Visible = true; }

    virtual bool OnLeftMouseButtonDoubleClick() override;
};

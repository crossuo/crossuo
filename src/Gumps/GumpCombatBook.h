// MIT License
// Copyright (C) December 2016 Hotride

#pragma once

#include "Gump.h"
#include "../Globals.h" // MAX_ABILITIES_COUNT

class CGumpCombatBook : public CGump
{
    int DictionaryPagesCount = 6;
    int AbilityCount = MAX_ABILITIES_COUNT;
    int PagesCount = 37;

private:
    const int ID_GCB_BUTTON_PREV = 1;
    const int ID_GCB_BUTTON_NEXT = 2;
    const int ID_GCB_BUTTON_MINIMIZE = 3;
    const int ID_GCB_LOCK_MOVING = 4;
    const int ID_GCB_ICON_FIRST = 5;
    const int ID_GCB_ICON_SECOND = 6;
    const int ID_GCB_ICON = 100;

    static const astr_t m_AbilityName[MAX_ABILITIES_COUNT];

    CGUIButton *m_PrevPage{ nullptr };
    CGUIButton *m_NextPage{ nullptr };

public:
    CGumpCombatBook(int x, int y);
    virtual ~CGumpCombatBook();

    static std::vector<uint16_t> GetItemsList(uint8_t index);

    virtual void DelayedClick(CRenderObject *obj) override;

    void ChangePage(int newPage);

    virtual void InitToolTip() override;

    virtual void PrepareContent() override;

    virtual void UpdateContent() override;

    GUMP_BUTTON_EVENT_H override;

    virtual bool OnLeftMouseButtonDoubleClick() override;
};

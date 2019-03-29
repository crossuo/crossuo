// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpBuff : public CGump
{
private:
    const int BUFF_ITEM_STEP_OFFSET_X = 3;
    const int BUFF_ITEM_STEP_OFFSET_Y = 3;
    const int MUNIMUM_ICON_ALPHA = 80;
    const int ALPHA_CHANGE_KOEFF = 600;
    const int USE_ALPHA_BLENDING_WHEN_TIMER_LESS = 10000;

    const int ID_GB_NEXT_WINDOW_DIRECTION = 1;
    const int ID_GB_LOCK_MOVING = 2;

    void GetGumpStatus(
        CPoint2Di &ball,
        CPoint2Di &items,
        bool &useX,
        bool &decX,
        bool &decY,
        CPoint2Di &startGump,
        CSize &endGump);

public:
    CGumpBuff(short x, short y);
    virtual ~CGumpBuff();

    virtual bool CanBeDisplayed() override;

    void AddBuff(uint16_t id, uint16_t timer, const wstring &text);

    void DeleteBuff(uint16_t id);

    void UpdateBuffIcons();

    virtual void InitToolTip() override;

    virtual void PrepareContent() override;

    virtual void UpdateContent() override;

    GUMP_BUTTON_EVENT_H override;
};

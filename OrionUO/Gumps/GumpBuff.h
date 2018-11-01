/***********************************************************************************
**
** GumpBuff.h
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef GUMPBUFF_H
#define GUMPBUFF_H

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
    const int ID_GB_BUFF_ITEM = 10;

    void GetGumpStatus(
        Wisp::CPoint2Di &ball,
        Wisp::CPoint2Di &items,
        bool &useX,
        bool &decX,
        bool &decY,
        Wisp::CPoint2Di &startGump,
        Wisp::CSize &endGump);

public:
    CGumpBuff(short x, short y);
    virtual ~CGumpBuff();

    virtual bool CanBeDisplayed();

    void AddBuff(uint16_t id, uint16_t timer, const wstring &text);

    void DeleteBuff(uint16_t id);

    void UpdateBuffIcons();

    virtual void InitToolTip();

    virtual void PrepareContent();

    virtual void UpdateContent();

    GUMP_BUTTON_EVENT_H;
};

#endif

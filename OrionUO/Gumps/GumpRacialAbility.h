// MIT License
// Copyright (C) January 2017 Hotride

#pragma once

class CGumpRacialAbility : public CGump
{
private:
    const int ID_GS_LOCK_MOVING = 1;

public:
    CGumpRacialAbility(int serial, int x, int y);
    virtual ~CGumpRacialAbility();

    static void OnAbilityUse(uint16_t index);

    virtual void InitToolTip();

    GUMP_BUTTON_EVENT_H;

    virtual bool OnLeftMouseButtonDoubleClick();
};

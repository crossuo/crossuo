// MIT License
// Copyright (C) December 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpAbility : public CGump
{
private:
    const int ID_GS_LOCK_MOVING = 1;

    CGUIGumppic *m_Body{ nullptr };
    CGUIGlobalColor *m_Colorizer{ nullptr };

public:
    CGumpAbility(int serial, int x, int y);
    virtual ~CGumpAbility();

    static void OnAbilityUse(int index);

    virtual void UpdateContent();

    virtual void InitToolTip();

    GUMP_BUTTON_EVENT_H;

    virtual bool OnLeftMouseButtonDoubleClick();
};

// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpSkill : public CGump
{
private:
    const int ID_GS_LOCK_MOVING = 1;

public:
    CGumpSkill(int serial, int x, int y);
    virtual ~CGumpSkill();

    GUMP_BUTTON_EVENT_H;

    virtual void OnLeftMouseButtonUp();
};

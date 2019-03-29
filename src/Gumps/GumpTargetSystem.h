// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpTargetSystem : public CGump
{
    string OldName = "";

private:
    const int ID_GSB_LOCK_MOVING = 2;

    CGUIGumppic *m_Body{ nullptr };
    CGUIDataBox *m_DataBox{ nullptr };

public:
    CGumpTargetSystem(uint32_t serial, short x, short y);
    virtual ~CGumpTargetSystem();

    virtual bool CanBeDisplayed() override;

    virtual void PrepareContent() override;
    virtual void UpdateContent() override;

    GUMP_BUTTON_EVENT_H override;

    virtual void OnLeftMouseDown();
    virtual bool OnLeftMouseButtonDoubleClick() override;
};

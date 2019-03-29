// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpScreenCharacterList : public CGump
{
public:
    CGumpScreenCharacterList();
    virtual ~CGumpScreenCharacterList();

    virtual void UpdateContent() override;
    virtual void InitToolTip() override;

    GUMP_BUTTON_EVENT_H override;
    GUMP_TEXT_ENTRY_EVENT_H override;

    virtual bool OnLeftMouseButtonDoubleClick() override;
};

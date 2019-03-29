// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpScreenServer : public CGump
{
public:
    CGumpScreenServer();
    virtual ~CGumpScreenServer();

    virtual void UpdateContent() override;
    virtual void InitToolTip() override;

    GUMP_BUTTON_EVENT_H override;
    GUMP_TEXT_ENTRY_EVENT_H override;
};

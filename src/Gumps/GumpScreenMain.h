// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpScreenMain : public CGump
{
    CGUIButton *m_Arrow{ nullptr };

public:
    CGumpScreenMain();
    virtual ~CGumpScreenMain();

    CEntryText *m_PasswordFake{ nullptr };

    virtual void PrepareContent() override;
    virtual void UpdateContent() override;
    virtual void InitToolTip() override;

    GUMP_BUTTON_EVENT_H override;
    GUMP_TEXT_ENTRY_EVENT_H override;
};

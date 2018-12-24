// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

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

    GUMP_BUTTON_EVENT_H;
    GUMP_TEXT_ENTRY_EVENT_H;
};

// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGumpScreenServer : public CGump
{
public:
    CGumpScreenServer();
    virtual ~CGumpScreenServer();

    virtual void UpdateContent() override;
    virtual void InitToolTip() override;

    GUMP_BUTTON_EVENT_H;
    GUMP_TEXT_ENTRY_EVENT_H;
};

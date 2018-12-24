// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGumpMenubar : public CGump
{
    bool m_Opened = true;

public:
    CGumpMenubar(short x, short y);
    virtual ~CGumpMenubar();

    bool GetOpened() { return m_Opened; };
    void SetOpened(bool val);

    virtual bool CanBeDisplayed() override { return false; }
    virtual void InitToolTip() override;

    GUMP_BUTTON_EVENT_H;
    GUMP_TEXT_ENTRY_EVENT_H;

    virtual void OnLeftMouseButtonUp() override;
};

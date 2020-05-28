// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpConsoleType : public CGump
{
protected:
    bool m_ShowFullText = false;
    int m_SelectedType = 0;
    virtual void CalculateGumpState() override;

public:
    CGumpConsoleType(bool minimized, bool showFullText);
    virtual ~CGumpConsoleType();

    virtual bool CanBeDisplayed() override;
    virtual void UpdateContent() override;
    virtual void InitToolTip() override;

    void SetConsolePrefix() const;
    bool ConsoleIsEmpty() const;
    bool GetShowFullText() const { return m_ShowFullText; };
    void SetShowFullText(bool val);

    GUMP_BUTTON_EVENT_H override;
    GUMP_CHECKBOX_EVENT_H override;
    GUMP_TEXT_ENTRY_EVENT_H override;
};

extern CGumpConsoleType *g_GumpConsoleType;

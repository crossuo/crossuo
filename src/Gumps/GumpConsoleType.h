// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"

class CGumpConsoleType : public CGump
{
protected:
    bool m_ShowFullText = false;
    int m_SelectedType = 0;

    virtual void CalculateGumpState();

public:
    CGumpConsoleType(bool minimized, bool showFullText);
    virtual ~CGumpConsoleType();

    virtual bool CanBeDisplayed();
    virtual void UpdateContent();
    virtual void InitToolTip();

    bool ConsoleIsEmpty();
    void DeleteConsolePrefix();
    void SetConsolePrefix();
    bool GetShowFullText() { return m_ShowFullText; };
    void SetShowFullText(bool val);

    GUMP_BUTTON_EVENT_H;
    GUMP_CHECKBOX_EVENT_H;
    GUMP_TEXT_ENTRY_EVENT_H;
};

extern CGumpConsoleType *g_GumpConsoleType;

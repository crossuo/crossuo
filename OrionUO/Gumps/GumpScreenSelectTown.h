// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGumpScreenSelectTown : public CGump
{
private:
    vector<Wisp::CPoint2Di> m_TownButtonText;
    CGUIHTMLGump *m_HTMLGump{ nullptr };
    CGUIText *m_Description{ nullptr };

public:
    CGumpScreenSelectTown();
    virtual ~CGumpScreenSelectTown();

    virtual void UpdateContent();

    GUMP_BUTTON_EVENT_H;
    GUMP_TEXT_ENTRY_EVENT_H;
};

// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGumpScreenSelectTown : public CGump
{
private:
    static const int ID_STS_QUIT = 1;
    static const int ID_STS_ARROW_PREV = 2;
    static const int ID_STS_ARROW_NEXT = 3;
    static const int ID_STS_HTML_GUMP = 4;
    static const int ID_STS_TOWN = 10;

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

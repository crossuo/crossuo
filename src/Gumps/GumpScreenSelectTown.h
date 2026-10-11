// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Gump.h"
#include "Gump1998ScrollChoice.h"

class CGUISlider;
class CGUIText;
class CGUIGumppic;

class CGumpScreenSelectTown : public CGump
{
private:
    std::vector<CPoint2Di> m_TownButtonText;
    CGUIHTMLGump *m_HTMLGump{ nullptr };
    CGUIText *m_Description{ nullptr };

    //1.25.32 (1998) CityChoice (server + start location, one gump): the
    //side scrolls are the collapsed lists, hidden while their popup is
    //open (see UpdateContent1998 / GUMP_BUTTON_EVENT_C)
    void UpdateContent1998();

    //the open popup: 0 none, 1 location, 2 server
    int m_1998OpenPopup = 0;

    //the two scrollchoice popups (stateful across rebuilds; each owns a
    //disjoint id block, see GUMP_BUTTON_EVENT_C)
    C1998ScrollChoice m_1998LocationPopup;
    C1998ScrollChoice m_1998ServerPopup;

public:
    CGumpScreenSelectTown();
    virtual ~CGumpScreenSelectTown();

    virtual void UpdateContent() override;

    GUMP_BUTTON_EVENT_H override;
    GUMP_TEXT_ENTRY_EVENT_H override;
    GUMP_SLIDER_CLICK_EVENT_H override;
    GUMP_SLIDER_MOVE_EVENT_H override;
};

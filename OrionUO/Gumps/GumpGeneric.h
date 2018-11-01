/***********************************************************************************
**
** GumpGeneric.h
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#pragma once
#include "Input.h"

class CGumpGeneric : public CGump
{
public:
    uint32_t MasterGump = 0;

    CGumpGeneric(uint32_t serial, short x, short y, uint32_t id);
    virtual ~CGumpGeneric();

    void AddText(
        int index,
        const wstring &text,
        CBaseGUI *start = nullptr,
        bool backbroundCanBeColored = false);

    void SendGumpResponse(int index);

    virtual void InitToolTip();

    GUMP_BUTTON_EVENT_H;
    GUMP_DIRECT_HTML_LINK_EVENT_H;

    virtual bool OnLeftMouseButtonDoubleClick();

#if USE_WISP
    virtual void OnCharPress(const WPARAM &wParam, const LPARAM &lParam);
#else
    virtual void OnTextInput(const SDL_TextInputEvent &ev) override;
#endif
    virtual void OnKeyDown(const KeyEvent &ev) override;
};

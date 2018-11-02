/***********************************************************************************
**
** GumpDrag.h
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#pragma once
#include "Input.h"

class CGumpDrag : public CGump
{
private:
    static const int ID_GD_SCROLL = 1;
    static const int ID_GD_OKAY = 2;
    static const int ID_GD_TEXT_FIELD = 3;

    bool m_StartText{ true };

    CGUITextEntry *m_Entry{ nullptr };
    CGUISlider *m_Slider{ nullptr };

public:
    CGumpDrag(uint32_t serial, short x, short y);
    virtual ~CGumpDrag();

    void UpdateContent();

    GUMP_BUTTON_EVENT_H;
    GUMP_SLIDER_CLICK_EVENT_H;
    GUMP_SLIDER_MOVE_EVENT_H;

    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;

    void OnOkayPressed();
};

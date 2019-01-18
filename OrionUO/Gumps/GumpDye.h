// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GumpSelectColor.h"

class CGumpDye : public CGumpSelectColor
{
private:
    uint16_t GetCurrentColor();
    CGUITilepic *m_Tube{ nullptr };

public:
    CGumpDye(uint32_t serial, int16_t x, int16_t y, uint16_t graphic);
    virtual ~CGumpDye();

    virtual void UpdateContent();
    virtual void OnSelectColor(uint16_t color);

    GUMP_BUTTON_EVENT_H;
    GUMP_SLIDER_CLICK_EVENT_H;
    GUMP_SLIDER_MOVE_EVENT_H;
};

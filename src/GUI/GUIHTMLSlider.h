// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUISlider.h"

class CGUIHTMLSlider : public CGUISlider
{
private:
    class CGUIHTMLGump *m_HTMLGump{ nullptr };

public:
    CGUIHTMLSlider(
        class CGUIHTMLGump *htmlGump,
        int serial,
        uint16_t graphic,
        uint16_t graphicSelected,
        uint16_t graphicPressed,
        uint16_t backgroundGraphic,
        bool compositeBackground,
        bool vertical,
        int x,
        int y,
        int length,
        int minValue,
        int maxValue,
        int value);
    virtual ~CGUIHTMLSlider();
    virtual void CalculateOffset();
    virtual bool IsControlHTML() { return true; }
};

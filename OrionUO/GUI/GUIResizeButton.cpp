// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIResizeButton.h"

CGUIResizeButton::CGUIResizeButton(
    int serial, uint16_t graphic, uint16_t graphicSelected, uint16_t graphicPressed, int x, int y)
    : CGUIButton(serial, graphic, graphicSelected, graphicPressed, x, y)
{
    Type = GOT_RESIZEBUTTON;
}

CGUIResizeButton::~CGUIResizeButton()
{
}

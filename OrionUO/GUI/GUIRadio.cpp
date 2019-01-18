// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIRadio.h"

CGUIRadio::CGUIRadio(
    int serial, uint16_t graphic, uint16_t graphicChecked, uint16_t graphicDisabled, int x, int y)
    : CGUICheckbox(serial, graphic, graphicChecked, graphicDisabled, x, y)
{
    Type = GOT_RADIO;
}

CGUIRadio::~CGUIRadio()
{
}

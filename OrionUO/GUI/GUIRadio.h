// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUICheckbox.h"

class CGUIRadio : public CGUICheckbox
{
public:
    CGUIRadio(
        int serial,
        uint16_t graphic,
        uint16_t graphicChecked,
        uint16_t graphicDisabled,
        int x,
        int y);
    virtual ~CGUIRadio();
};

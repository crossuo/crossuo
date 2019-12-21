// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include <common/str.h>
#include "GUIText.h"

class CGUIComboboxText : public CGUIText
{
public:
    CGUIComboboxText(
        uint16_t color,
        uint8_t font,
        const astr_t &str,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);
    CGUIComboboxText(
        uint16_t color,
        uint8_t font,
        const wstr_t &str,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);
    virtual ~CGUIComboboxText();

    virtual void OnMouseEnter();
    virtual void OnMouseExit();
};

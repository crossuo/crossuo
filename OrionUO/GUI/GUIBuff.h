// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUIDrawObject.h"

class CGUIBuff : public CGUIDrawObject
{
public:
    uint32_t Timer = 0;
    wstring Text = {};
    uint32_t TooltipTimer = 0;
    bool DecAlpha = true;
    uint8_t Alpha = 0xFF;

    CGUIBuff(uint16_t graphic, int timer, const wstring &text);
    virtual ~CGUIBuff();
    virtual void Draw(bool checktrans = false);
};

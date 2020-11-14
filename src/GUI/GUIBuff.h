// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <common/str.h>
#include "GUIDrawObject.h"

class CGUIBuff : public CGUIDrawObject
{
public:
    uint32_t Timer = 0;
    wstr_t Text;
    uint32_t TooltipTimer = 0;
    bool DecAlpha = true;
    uint8_t Alpha = 0xFF;

    CGUIBuff(uint16_t graphic, int timer, const wstr_t &text);
    virtual ~CGUIBuff();
    virtual void Draw(bool checktrans = false);
};

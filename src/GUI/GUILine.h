// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "BaseGUI.h"

class CGUILine : public CBaseGUI
{
    //! Target X coordinate
    int TargetX = 0;

    //! Target Y coordinate
    int TargetY = 0;

    //! R color component
    uint8_t ColorR = 0;

    //! G color component
    uint8_t ColorG = 0;

    //! B color component
    uint8_t ColorB = 0;

    //! Alpha value
    uint8_t ColorA = 0;

public:
    CGUILine(int x, int y, int targetX, int targetY, int polygoneColor);
    virtual ~CGUILine() override;

    virtual void Draw(bool checktrans = false) override;
};

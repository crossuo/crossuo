// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "BaseGUI.h"

class CGUIGlobalColor : public CBaseGUI
{
    //! R color component
    float R = 0.0f;

    //! G color component
    float G = 0.0f;

    //! B color component
    float B = 0.0f;

    //! A color component
    float A = 0.0f;

public:
    CGUIGlobalColor(bool enabled, float r, float g, float b, float a);
    virtual ~CGUIGlobalColor() override;

    virtual void Draw(bool checktrans = false) override;
};

// MIT License
// Copyright (C) December 2016 Hotride

#pragma once

#include "BaseGUI.h"

class CGUIGlobalColor : public CBaseGUI
{
    //!R компонента цвета
    float R = 0.0f;

    //!G компонента цвета
    float G = 0.0f;

    //!B компонента цвета
    float B = 0.0f;

    //!A компонента цвета
    float A = 0.0f;

public:
    CGUIGlobalColor(bool enabled, float r, float g, float b, float a);
    virtual ~CGUIGlobalColor();

    virtual void Draw(bool checktrans = false);
};

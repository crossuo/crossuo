// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseGUI.h"

struct CGUIAlphaBlending : public CBaseGUI
{
    bool Enabled = false;
    float Alpha = 0.0f;
    CGUIAlphaBlending(bool enabled, float alpha);
    virtual ~CGUIAlphaBlending(){};
    virtual void Draw(bool checktrans = false);
};

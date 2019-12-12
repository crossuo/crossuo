// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseGUI.h"
#include "GUIBlending.h"

#ifndef NEW_RENDERER_ENABLED
class CGUIAlphaBlending : public CGUIBlending
#else
class CGUIAlphaBlending : public CBaseGUI
#endif
{
public:
    //!Значение для установки альфа-канала
    float Alpha = 0.0f;

    CGUIAlphaBlending(bool enabled, float alpha);
    virtual ~CGUIAlphaBlending();

    virtual void Draw(bool checktrans = false);
};

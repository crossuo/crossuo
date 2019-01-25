// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUIBlending.h"

class CGUIAlphaBlending : public CGUIBlending
{
public:
    //!Значение для установки альфа-канала
    float Alpha = 0.0f;

    CGUIAlphaBlending(bool enabled, float alpha);
    virtual ~CGUIAlphaBlending();

    virtual void Draw(bool checktrans = false);
};

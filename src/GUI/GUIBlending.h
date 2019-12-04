// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#ifndef NEW_RENDERER_ENABLED

#include "BaseGUI.h"
#include "../Backend.h"

class CGUIBlending : public CBaseGUI
{
    uint32_t SFactor = GL_SRC_ALPHA;           // FIXME
    uint32_t DFactor = GL_ONE_MINUS_SRC_ALPHA; // FIXME

public:
    CGUIBlending(bool enabled, uint32_t sFactor, uint32_t dFactor);
    virtual ~CGUIBlending();

    virtual void Draw(bool checktrans = false);
};

#endif // #ifndef NEW_RENDERER_ENABLED
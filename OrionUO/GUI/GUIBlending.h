// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseGUI.h"
#include "../Backend.h"

class CGUIBlending : public CBaseGUI
{
    //!Функция для исходных пикселей
    GLenum SFactor = GL_SRC_ALPHA;

    //!Функция для входных пикселей
    GLenum DFactor = GL_ONE_MINUS_SRC_ALPHA;

public:
    CGUIBlending(bool enabled, GLenum sFactor, GLenum dFactor);
    virtual ~CGUIBlending();

    virtual void Draw(bool checktrans = false);
};

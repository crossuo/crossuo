// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseGUI.h"

class CGUIShader : public CBaseGUI
{
private:
    CGLShader *m_Shader{ nullptr };

public:
    CGUIShader(CGLShader *shader, bool enabled);
    virtual ~CGUIShader();

    virtual void Draw(bool checktrans = false);
};

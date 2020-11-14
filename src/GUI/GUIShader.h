// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "BaseGUI.h"
struct CGLShader; // REMOVE

struct CGUIShader : public CBaseGUI
{
    CGLShader *m_Shader = nullptr;
    CGUIShader(CGLShader *shader, bool enabled);
    virtual ~CGUIShader() = default;
    virtual void Draw(bool checktrans = false);
};

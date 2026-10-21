// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "BaseGUI.h"
struct CRenderShader;

struct CGUIShader : public CBaseGUI
{
    CRenderShader *m_Shader = nullptr;
    CGUIShader(CRenderShader *shader, bool enabled);
    virtual ~CGUIShader() override = default;
    virtual void Draw(bool checktrans = false) override;
};

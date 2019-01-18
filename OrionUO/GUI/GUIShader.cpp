// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIShader.h"

CGUIShader::CGUIShader(CGLShader *shader, bool enabled)
    : CBaseGUI(GOT_SHADER, 0, 0, 0, 0, 0)
    , m_Shader(shader)
{
    Enabled = enabled;
}

CGUIShader::~CGUIShader()
{
}

void CGUIShader::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    if (Enabled && m_Shader != nullptr)
    {
        m_Shader->Use();
    }
    else
    {
        UnuseShader();
    }
}

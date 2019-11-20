// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIShader.h"
#include "../Utility/PerfMarker.h"

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
    ScopedPerfMarker(__FUNCTION__);
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

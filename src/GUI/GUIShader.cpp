// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIShader.h"
#include "../Utility/PerfMarker.h"
#include "../GLEngine/GLShader.h" // REMOVE

CGUIShader::CGUIShader(CGLShader *shader, bool enabled)
    : CBaseGUI(GOT_SHADER, 0, 0, 0, 0, 0)
    , m_Shader(shader)
{
    Enabled = enabled;
}

void CGUIShader::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
    if (Enabled && m_Shader != nullptr)
    {
        m_Shader->Enable();
    }
    else
    {
        m_Shader->Disable();
    }
}

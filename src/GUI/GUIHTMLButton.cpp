// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIHTMLButton.h"
#include "GUIHTMLGump.h"
#include "../Renderer/RenderAPI.h"
#include "../Globals.h" // g_ShaderDrawMode

CGUIHTMLButton::CGUIHTMLButton(
    CGUIHTMLGump *htmlGump,
    int serial,
    uint16_t graphic,
    uint16_t graphicSelected,
    uint16_t graphicPressed,
    int x,
    int y)
    : CGUIButton(serial, graphic, graphicSelected, graphicPressed, x, y)
    , m_HTMLGump(htmlGump)
{
}

CGUIHTMLButton::~CGUIHTMLButton()
{
}

void CGUIHTMLButton::SetShaderMode()
{
#ifndef NEW_RENDERER_ENABLED
    glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
#else
    ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
    cmd.value.asInt1 = SDM_NO_COLOR;
    RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
}

void CGUIHTMLButton::Scroll(bool up, int delay)
{
    if (m_HTMLGump != nullptr)
    {
        m_HTMLGump->Scroll(up, delay);
    }
}

// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIHTMLButton.h"
#include "GUIHTMLGump.h"
#include "Renderer/RenderAPI.h"

extern RenderCmdList *g_renderCmdList;

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
    auto uniformValue = SDM_NO_COLOR;
    RenderAdd_SetShaderUniform(
        g_renderCmdList,
        &ShaderUniformCmd(
            g_ShaderDrawMode, &uniformValue, ShaderUniformType::ShaderUniformType_Int1));
#endif
}

void CGUIHTMLButton::Scroll(bool up, int delay)
{
    if (m_HTMLGump != nullptr)
    {
        m_HTMLGump->Scroll(up, delay);
    }
}

// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIHTMLButton.h"
#include "GUIHTMLGump.h"

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
    glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
}

void CGUIHTMLButton::Scroll(bool up, int delay)
{
    if (m_HTMLGump != nullptr)
    {
        m_HTMLGump->Scroll(up, delay);
    }
}

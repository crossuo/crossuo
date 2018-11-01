// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** GUIDrawObject.cpp
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#include "stdafx.h"

CGUIDrawObject::CGUIDrawObject(
    GUMP_OBJECT_TYPE type, int serial, uint16_t graphic, uint16_t color, int x, int y)
    : CBaseGUI(type, serial, graphic, color, x, y)
{
}

CGUIDrawObject::~CGUIDrawObject()
{
}

Wisp::CSize CGUIDrawObject::GetSize()
{
    DEBUG_TRACE_FUNCTION;
    Wisp::CSize size;
    CGLTexture *th = g_Orion.ExecuteGump(GetDrawGraphic());

    if (th != nullptr)
    {
        size.Width = th->Width;
        size.Height = th->Height;
    }

    return size;
}

void CGUIDrawObject::SetShaderMode()
{
    DEBUG_TRACE_FUNCTION;

    if (Color != 0)
    {
        if (PartialHue)
            glUniform1iARB(g_ShaderDrawMode, SDM_PARTIAL_HUE);
        else
            glUniform1iARB(g_ShaderDrawMode, SDM_COLORED);

        g_ColorManager.SendColorsToShader(Color);
    }
    else
        glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
}

void CGUIDrawObject::PrepareTextures()
{
    DEBUG_TRACE_FUNCTION;
    g_Orion.ExecuteGump(Graphic);
}

void CGUIDrawObject::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    CGLTexture *th = g_Orion.ExecuteGump(GetDrawGraphic());

    if (th != nullptr)
    {
        SetShaderMode();

        th->Draw(m_X, m_Y, checktrans);
    }
}

bool CGUIDrawObject::Select()
{
    DEBUG_TRACE_FUNCTION;
    CGLTexture *th = g_Orion.ExecuteGump(Graphic);

    if (th != nullptr)
        return th->Select(m_X, m_Y, !CheckPolygone);

    return false;
}


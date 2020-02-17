// MIT License
// Copyright (C) August 2016 Hotride

#include "GUILine.h"
#include "../Renderer/RenderAPI.h"
#include "../Utility/PerfMarker.h"
#include "../Globals.h" // ToColor*

CGUILine::CGUILine(int startX, int startY, int targetX, int targetY, int polygoneColor)
    : CBaseGUI(GOT_LINE, 0, 0, 0, startX, startY)
    , TargetX(targetX)
    , TargetY(targetY)
{
    ColorR = ToColorR(polygoneColor);
    ColorG = ToColorG(polygoneColor);
    ColorB = ToColorB(polygoneColor);
    ColorA = ToColorA(polygoneColor);

    if (ColorA == 0u)
    {
        ColorA = 0xFF;
    }
}

CGUILine::~CGUILine()
{
}

void CGUILine::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
#ifndef NEW_RENDERER_ENABLED
    glColor4ub(ColorR, ColorG, ColorB, ColorA);

    if (ColorA < 0xFF)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        g_GL.DrawLine(m_X, m_Y, TargetX, TargetY);

        glDisable(GL_BLEND);
    }
    else
    {
        g_GL.DrawLine(m_X, m_Y, TargetX, TargetY);
    }

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
#else
    RenderAdd_DrawLine(
        g_renderCmdList,
        DrawLineCmd{ m_X,
                     m_Y,
                     TargetX,
                     TargetY,
                     { ColorR / 255.f, ColorG / 255.f, ColorB / 255.f, ColorA / 255.f } });
#endif
}

// MIT License
// Copyright (C) August 2016 Hotride

#include "GUILine.h"

CGUILine::CGUILine(int startX, int startY, int targetX, int targetY, int polygoneColor)
    : CBaseGUI(GOT_LINE, 0, 0, 0, startX, startY)
    , TargetX(targetX)
    , TargetY(targetY)
{
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
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
}

// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIColoredPolygone.h"
#include "../SelectedObject.h"
#include "../Gumps/Gump.h"

CGUIColoredPolygone::CGUIColoredPolygone(
    int serial, uint16_t color, int x, int y, int width, int height, int polygoneColor)
    : CGUIPolygonal(GOT_COLOREDPOLYGONE, x, y, width, height)
{
    DEBUG_TRACE_FUNCTION;
    Serial = serial;
    UpdateColor(color, polygoneColor);
}

CGUIColoredPolygone::~CGUIColoredPolygone()
{
}

void CGUIColoredPolygone::UpdateColor(uint16_t color, int polygoneColor)
{
    DEBUG_TRACE_FUNCTION;
    Color = color;

    ColorR = ToColorR(polygoneColor);
    ColorG = ToColorG(polygoneColor);
    ColorB = ToColorB(polygoneColor);
    ColorA = ToColorA(polygoneColor);

    if (ColorA == 0u)
    {
        ColorA = 0xFF;
    }
}

void CGUIColoredPolygone::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    glColor4ub(ColorR, ColorG, ColorB, ColorA);

    if (ColorA < 0xFF)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        g_GL.DrawPolygone(m_X, m_Y, Width, Height);

        glDisable(GL_BLEND);
    }
    else
    {
        g_GL.DrawPolygone(m_X, m_Y, Width, Height);
    }

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    if (Focused || (DrawDot && g_GumpSelectedElement == this))
    {
        g_GL.DrawPolygone(m_X + (Width / 2) - 1, m_Y + (Height / 2) - 1, 2, 2);
    }
}

void CGUIColoredPolygone::OnMouseEnter()
{
    DEBUG_TRACE_FUNCTION;
    if (DrawDot && g_SelectedObject.Gump != nullptr)
    {
        g_SelectedObject.Gump->WantRedraw = true;
    }
}

void CGUIColoredPolygone::OnMouseExit()
{
    DEBUG_TRACE_FUNCTION;
    if (DrawDot && g_LastSelectedObject.Gump != nullptr)
    {
        g_LastSelectedObject.Gump->WantRedraw = true;
    }
}

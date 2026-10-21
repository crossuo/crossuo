// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GUILine.h"
#include "../Renderer/RenderAPI.h"
#include "../Utility/PerfMarker.h"
#include "../Globals.h" // ToColor*
#include "GUI/BaseGUI.h"

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
= default;

void CGUILine::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
    SCOPED_GL_DEBUG_MARKER();
    RenderAdd_DrawLine(
        g_renderCmdList,
        DrawLineCmd{ m_X,
                     m_Y,
                     TargetX,
                     TargetY,
                     { ColorR / 255.f, ColorG / 255.f, ColorB / 255.f, ColorA / 255.f } });
}

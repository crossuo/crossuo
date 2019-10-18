// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIResizepic.h"
#include "../CrossUO.h"
#include "Renderer/RenderAPI.h"
#include "Utility/PerfMarker.h"

extern RenderCmdList *g_renderCmdList;

CGUIResizepic::CGUIResizepic(int serial, uint16_t graphic, int x, int y, int width, int height)
    : CGUIPolygonal(GOT_RESIZEPIC, x, y, width, height)
{
    Serial = serial;
    Graphic = graphic;
}

void CGUIResizepic::PrepareTextures()
{
    DEBUG_TRACE_FUNCTION;
    g_Game.ExecuteResizepic(Graphic);
}

void CGUIResizepic::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
    DEBUG_TRACE_FUNCTION;

#ifndef NEW_RENDERER_ENABLED
    glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
#else
    auto uniformValue = SDM_NO_COLOR;
    RenderAdd_SetShaderUniform(
        g_renderCmdList,
        &ShaderUniformCmd(g_ShaderDrawMode, &uniformValue, ShaderUniformType::Int1));
#endif
    g_Game.DrawResizepicGump(Graphic, m_X, m_Y, Width, Height, checktrans);
}

bool CGUIResizepic::Select()
{
    DEBUG_TRACE_FUNCTION;
    if (BoundingBoxCheck)
    {
        return CGUIPolygonal::Select();
    }
    return g_Game.ResizepicPixelsInXY(Graphic, m_X, m_Y, Width, Height);
}

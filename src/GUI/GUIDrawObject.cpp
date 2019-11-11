// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIDrawObject.h"
#include "../CrossUO.h"
#include "../Point.h"
#include "../Managers/ColorManager.h"
#include "Renderer/RenderAPI.h"
#include "Utility/PerfMarker.h"

extern RenderCmdList *g_renderCmdList;

CGUIDrawObject::CGUIDrawObject(
    GUMP_OBJECT_TYPE type, int serial, uint16_t graphic, uint16_t color, int x, int y)
    : CBaseGUI(type, serial, graphic, color, x, y)
{
}

CGUIDrawObject::~CGUIDrawObject()
{
}

CSize CGUIDrawObject::GetSize()
{
    DEBUG_TRACE_FUNCTION;
    CSize size;
    auto spr = g_Game.ExecuteGump(GetDrawGraphic());
    if (spr != nullptr)
    {
        size.Width = spr->Width;
        size.Height = spr->Height;
    }
    return size;
}

void CGUIDrawObject::SetShaderMode()
{
    DEBUG_TRACE_FUNCTION;
    if (Color != 0)
    {
        auto uniformValue = PartialHue ? SDM_PARTIAL_HUE : SDM_COLORED;
#ifndef NEW_RENDERER_ENABLED
        glUniform1iARB(g_ShaderDrawMode, uniformValue);
#else
        RenderAdd_SetShaderUniform(
            g_renderCmdList,
            &ShaderUniformCmd(
                g_ShaderDrawMode, &uniformValue, ShaderUniformType::ShaderUniformType_Int1));
#endif
        g_ColorManager.SendColorsToShader(Color);
    }
    else
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
}

void CGUIDrawObject::PrepareTextures()
{
    DEBUG_TRACE_FUNCTION;
    g_Game.ExecuteGump(Graphic);
}

void CGUIDrawObject::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
    DEBUG_TRACE_FUNCTION;
    auto spr = g_Game.ExecuteGump(GetDrawGraphic());
    if (spr != nullptr && spr->Texture)
    {
        SetShaderMode();
        spr->Texture->Draw(m_X, m_Y, checktrans);
    }
}

bool CGUIDrawObject::Select()
{
    DEBUG_TRACE_FUNCTION;
    auto spr = g_Game.ExecuteGump(Graphic);
    if (spr != nullptr)
    {
        return spr->Select(m_X, m_Y, !BoundingBoxCheck);
    }
    return false;
}

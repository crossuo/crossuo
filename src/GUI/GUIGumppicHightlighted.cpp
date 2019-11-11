// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIGumppicHightlighted.h"
#include "../SelectedObject.h"
#include "../Gumps/Gump.h"
#include "../Managers/ColorManager.h"
#include "Renderer/RenderAPI.h"

extern RenderCmdList *g_renderCmdList;

CGUIGumppicHightlighted::CGUIGumppicHightlighted(
    int serial, uint16_t graphic, uint16_t color, uint16_t selectedColor, int x, int y)
    : CGUIDrawObject(GOT_GUMPPICHIGHTLIGHTED, serial, graphic, color, x, y)
    , SelectedColor(selectedColor)
{
}

CGUIGumppicHightlighted::~CGUIGumppicHightlighted()
{
}

void CGUIGumppicHightlighted::SetShaderMode()
{
    DEBUG_TRACE_FUNCTION;

    if (g_SelectedObject.Object == this)
    {
#ifndef NEW_RENDERER_ENABLED
        glUniform1iARB(g_ShaderDrawMode, SDM_COLORED);
#else
        auto uniformValue = SDM_COLORED;
        RenderAdd_SetShaderUniform(
            g_renderCmdList,
            &ShaderUniformCmd(
                g_ShaderDrawMode, &uniformValue, ShaderUniformType::ShaderUniformType_Int1));
#endif

        g_ColorManager.SendColorsToShader(SelectedColor);
    }
    else if (Color != 0)
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

void CGUIGumppicHightlighted::OnMouseEnter()
{
    DEBUG_TRACE_FUNCTION;
    if (g_SelectedObject.Gump != nullptr)
    {
        g_SelectedObject.Gump->WantRedraw = true;
    }
}

void CGUIGumppicHightlighted::OnMouseExit()
{
    DEBUG_TRACE_FUNCTION;
    if (g_LastSelectedObject.Gump != nullptr)
    {
        g_LastSelectedObject.Gump->WantRedraw = true;
    }
}

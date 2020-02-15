// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIGumppicHightlighted.h"
#include "../SelectedObject.h"
#include "../Gumps/Gump.h"
#include "../Managers/ColorManager.h"
#include "../Renderer/RenderAPI.h"
#include "../Globals.h" // g_ShaderDrawMode

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
    if (g_SelectedObject.Object == this)
    {
#ifndef NEW_RENDERER_ENABLED
        glUniform1iARB(g_ShaderDrawMode, SDM_COLORED);
#else
        ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
        cmd.value.asInt1 = SDM_COLORED;
        RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif

        g_ColorManager.SendColorsToShader(SelectedColor);
    }
    else if (Color != 0)
    {
        auto uniformValue = PartialHue ? SDM_PARTIAL_HUE : SDM_COLORED;
#ifndef NEW_RENDERER_ENABLED
        glUniform1iARB(g_ShaderDrawMode, uniformValue);
#else
        ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
        cmd.value.asInt1 = uniformValue;
        RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif

        g_ColorManager.SendColorsToShader(Color);
    }
    else
    {
#ifndef NEW_RENDERER_ENABLED
        glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
#else
        ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
        cmd.value.asInt1 = SDM_NO_COLOR;
        RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
    }
}

void CGUIGumppicHightlighted::OnMouseEnter()
{
    if (g_SelectedObject.Gump != nullptr)
    {
        g_SelectedObject.Gump->WantRedraw = true;
    }
}

void CGUIGumppicHightlighted::OnMouseExit()
{
    if (g_LastSelectedObject.Gump != nullptr)
    {
        g_LastSelectedObject.Gump->WantRedraw = true;
    }
}

// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIGumppicHightlighted.h"
#include "../SelectedObject.h"
#include "../Gumps/Gump.h"
#include "../Managers/ColorManager.h"

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
        glUniform1iARB(g_ShaderDrawMode, SDM_COLORED);

        g_ColorManager.SendColorsToShader(SelectedColor);
    }
    else if (Color != 0)
    {
        if (PartialHue)
        {
            glUniform1iARB(g_ShaderDrawMode, SDM_PARTIAL_HUE);
        }
        else
        {
            glUniform1iARB(g_ShaderDrawMode, SDM_COLORED);
        }

        g_ColorManager.SendColorsToShader(Color);
    }
    else
    {
        glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
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

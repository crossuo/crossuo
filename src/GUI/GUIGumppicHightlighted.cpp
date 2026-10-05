// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

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
    if (SelectedColor != 0) // was if (g_SelectedObject.Object == this) in case there is a bug
    {
        RenderAdd_SetDrawMode(g_renderCmdList, SetDrawModeCmd{SDM_COLORED});

        g_ColorManager.SendColorsToShader(SelectedColor);
    }
    else if (Color != 0)
    {
        auto uniformValue = PartialHue ? SDM_PARTIAL_HUE : SDM_COLORED;
        RenderAdd_SetDrawMode(g_renderCmdList, SetDrawModeCmd{uniformValue});

        g_ColorManager.SendColorsToShader(Color);
    }
    else
    {
        RenderAdd_SetDrawMode(g_renderCmdList, SetDrawModeCmd{SDM_NO_COLOR});
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

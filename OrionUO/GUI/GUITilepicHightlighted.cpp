// MIT License
// Copyright (C) August 2016 Hotride

#include "GUITilepicHightlighted.h"
#include "../OrionUO.h"
#include "../SelectedObject.h"
#include "../Managers/ColorManager.h"
#include "../Gumps/Gump.h"

CGUITilepicHightlighted::CGUITilepicHightlighted(
    int serial,
    uint16_t graphic,
    uint16_t color,
    uint16_t selectedColor,
    int x,
    int y,
    bool doubleDraw)
    : CGUITilepic(graphic, color, x, y)
    , SelectedColor(selectedColor)
    , DoubleDraw(doubleDraw)
{
    Type = GOT_TILEPICHIGHTLIGHTED;
    Serial = serial;
}

CGUITilepicHightlighted::~CGUITilepicHightlighted()
{
}

void CGUITilepicHightlighted::SetShaderMode()
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

void CGUITilepicHightlighted::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    CGLTexture *th = g_Orion.ExecuteStaticArt(Graphic);

    if (th != nullptr)
    {
        SetShaderMode();

        th->Draw(m_X, m_Y, checktrans);

        if (DoubleDraw)
        {
            th->Draw(m_X + 5, m_Y + 5, checktrans);
        }
    }
}

bool CGUITilepicHightlighted::Select()
{
    DEBUG_TRACE_FUNCTION;
    CGLTexture *th = g_Orion.m_StaticDataIndex[Graphic].Texture;

    if (th != nullptr)
    {
        int count = 1 + static_cast<int>(DoubleDraw);
        int offset = 0;

        for (int i = 0; i < count; i++)
        {
            if (th->Select(m_X + offset, m_Y + offset, !CheckPolygone))
            {
                return true;
            }

            offset = 5;
        }
    }

    return false;
}

void CGUITilepicHightlighted::OnMouseEnter()
{
    DEBUG_TRACE_FUNCTION;
    if (g_SelectedObject.Gump != nullptr)
    {
        g_SelectedObject.Gump->WantRedraw = true;
    }
}

void CGUITilepicHightlighted::OnMouseExit()
{
    DEBUG_TRACE_FUNCTION;
    if (g_LastSelectedObject.Gump != nullptr)
    {
        g_LastSelectedObject.Gump->WantRedraw = true;
    }
}

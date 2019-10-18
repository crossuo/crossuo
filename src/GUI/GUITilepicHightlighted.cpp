// MIT License
// Copyright (C) August 2016 Hotride

#include "GUITilepicHightlighted.h"
#include <xuocore/uodata.h>
#include "../CrossUO.h"
#include "../SelectedObject.h"
#include "../Managers/ColorManager.h"
#include "../Gumps/Gump.h"
#include "Utility/PerfMarker.h"

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
    ScopedPerfMarker(__FUNCTION__);
    DEBUG_TRACE_FUNCTION;
    auto spr = g_Game.ExecuteStaticArt(Graphic);
    if (spr != nullptr && spr->Texture != nullptr)
    {
        SetShaderMode();
        spr->Texture->Draw(m_X, m_Y, checktrans);
        if (DoubleDraw)
        {
            spr->Texture->Draw(m_X + 5, m_Y + 5, checktrans);
        }
    }
}

bool CGUITilepicHightlighted::Select()
{
    DEBUG_TRACE_FUNCTION;
    auto spr = (CSprite *)g_Index.m_Static[Graphic].UserData;
    if (spr != nullptr)
    {
        int count = 1 + static_cast<int>(DoubleDraw);
        int offset = 0;
        for (int i = 0; i < count; i++)
        {
            if (spr->Select(m_X + offset, m_Y + offset, !BoundingBoxCheck))
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

// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GUIDrawObject.h"
#include "../CrossUO.h"
#include "../Sprite.h"
#include "../Point.h"
#include "../Managers/ColorManager.h"
#include "../Renderer/RenderAPI.h"
#include "../Utility/PerfMarker.h"

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
    if (Color != 0)
    {
        auto uniformValue = PartialHue ? SDM_PARTIAL_HUE : SDM_COLORED;
#ifndef NEW_RENDERER_ENABLED
        glUniform1iARB(g_ShaderDrawMode, uniformValue);
#else
        RenderAdd_SetDrawMode(g_renderCmdList, SetDrawModeCmd{uniformValue});
#endif
        g_ColorManager.SendColorsToShader(Color);
    }
    else
    {
#ifndef NEW_RENDERER_ENABLED
        glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
#else
        RenderAdd_SetDrawMode(g_renderCmdList, SetDrawModeCmd{SDM_NO_COLOR});
#endif
    }
}

void CGUIDrawObject::PrepareTextures()
{
    g_Game.ExecuteGump(Graphic);
}

void CGUIDrawObject::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);

    auto spr = g_Game.ExecuteGump(GetDrawGraphic());
    if (spr != nullptr && spr->Texture)
    {
        SetShaderMode();
        static const bool trace = getenv("XUO_GL1_TRACE") != nullptr;
        if (trace)
        {
            Warning(Renderer, "ITEM gumpic graphic=%u tex=%u at=(%d,%d) size=(%ux%u)",
                     GetDrawGraphic(), spr->Texture->Texture, m_X, m_Y, spr->Width, spr->Height);
        }
        spr->Texture->Draw(m_X, m_Y, checktrans);
    }
    else
    {
        static const bool trace = getenv("XUO_GL1_TRACE") != nullptr;
        if (trace)
        {
            Warning(Renderer, "ITEM SKIP gumpic graphic=%u (spr=%p)", GetDrawGraphic(), (void *)spr);
        }
    }
}

bool CGUIDrawObject::Select()
{
    auto spr = g_Game.ExecuteGump(Graphic);
    if (spr != nullptr)
    {
        return spr->Select(m_X, m_Y, !BoundingBoxCheck);
    }
    return false;
}

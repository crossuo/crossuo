// MIT License
// Copyright (C) December 2016 Hotride

#include "GUITilepicScaled.h"
#include "../CrossUO.h"
#include "../Sprite.h"
#include "../Renderer/RenderAPI.h"
#include "../Utility/PerfMarker.h"
#include "../GLEngine/GLEngine.h" // REMOVE

extern RenderCmdList *g_renderCmdList;

CGUITilepicScaled::CGUITilepicScaled(
    uint16_t graphic, uint16_t color, int x, int y, int width, int height)
    : CGUITilepic(graphic, color, x, y)
{
}

CGUITilepicScaled::~CGUITilepicScaled()
{
}

void CGUITilepicScaled::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
    DEBUG_TRACE_FUNCTION;
    auto spr = g_Game.ExecuteStaticArt(Graphic);
    if (spr != nullptr && spr->Texture != nullptr)
    {
        SetShaderMode();
#ifndef NEW_RENDERER_ENABLED
        g_GL.Draw(*spr->Texture, m_X, m_Y);
#else
        auto textureCmd = DrawQuadCmd{ spr->Texture->Texture,
                                       m_X,
                                       m_Y,
                                       uint32_t(spr->Texture->Width),
                                       uint32_t(spr->Texture->Height) };
        RenderAdd_DrawQuad(g_renderCmdList, textureCmd);
#endif
    }
}

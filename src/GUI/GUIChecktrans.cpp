// GPL
// Copyright (C) December 2019 Nirad Jean-Martin Miljours

#include "GUIChecktrans.h"
#include "../Renderer/RenderAPI.h"
#include "../Utility/PerfMarker.h"

extern RenderCmdList *g_renderCmdList;

CGUIChecktrans::CGUIChecktrans(int x, int y, int width, int height)
    : CGUIPolygonal(GOT_CHECKTRANS, x, y, width, height)
{
}

CGUIChecktrans::~CGUIChecktrans()
{
}

void CGUIChecktrans::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);

#ifndef NEW_RENDERER_ENABLED
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.3f, 0.3f, 0.3f, 0.3f);
    g_GL.DrawPolygone(m_X, m_Y, Width, Height);
    glDisable(GL_BLEND);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
#else
    RenderAdd_SetBlend(
        g_renderCmdList,
        BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                       BlendFactor::BlendFactor_OneMinusSrcAlpha });
    RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ { 0.3f, 0.3f, 0.3f, 0.3f } });
    RenderAdd_DrawUntexturedQuad(
        g_renderCmdList, DrawUntexturedQuadCmd{ m_X, m_Y, uint32_t(Width), uint32_t(Height) });
    RenderAdd_DisableBlend(g_renderCmdList);
    RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ { 1.0f, 1.0f, 1.0f, 1.0f } });
#endif
}

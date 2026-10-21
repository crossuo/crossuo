// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2019 Nirad Jean-Martin Miljours
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "GUIChecktrans.h"
#include "../Renderer/RenderAPI.h"
#include "../Utility/PerfMarker.h"

CGUIChecktrans::CGUIChecktrans(int x, int y, int width, int height)
    : CGUIPolygonal(GOT_CHECKTRANS, x, y, width, height)
{
}

CGUIChecktrans::~CGUIChecktrans()
= default;

void CGUIChecktrans::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
    SCOPED_GL_DEBUG_MARKER();
    RenderAdd_SetBlend(
        g_renderCmdList,
        BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                       BlendFactor::BlendFactor_OneMinusSrcAlpha });
    RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ { 0.3f, 0.3f, 0.3f, 0.3f } });
    RenderAdd_DrawUntexturedQuad(
        g_renderCmdList, DrawUntexturedQuadCmd{ m_X, m_Y, uint32_t(Width), uint32_t(Height) });
    RenderAdd_DisableBlend(g_renderCmdList);
    RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ { 1.0f, 1.0f, 1.0f, 1.0f } });
}

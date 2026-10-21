// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GUIScissor.h"
#include "../Renderer/RenderAPI.h"
#include "../Utility/PerfMarker.h"

CGUIScissor::CGUIScissor(bool enabled, int baseX, int baseY, int x, int y, int width, int height)
    : CGUIPolygonal(GOT_SCISSOR, x, y, width, height)
    , BaseX(baseX)
    , BaseY(baseY)
{
    Enabled = enabled;
}

CGUIScissor::~CGUIScissor()
= default;

void CGUIScissor::PushGumpLocal(int x, int y, int width, int height)
{
    // Coordinates are gump-local; the renderer shifts them by the active gump's
    // draw origin (installed as the scissor origin provider), so nothing to do
    // here beyond forwarding. Kept as a named entry point so gump code has an
    // obvious call to reach for instead of Render_PushScissor.
    Render_PushScissor(x, y, (uint32_t)width, (uint32_t)height);
}

void CGUIScissor::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);

    if (Enabled)
    {
        PushGumpLocal(BaseX + m_X, BaseY + m_Y, Width, Height);
    }
    else
    {
        Render_PopScissor();
    }
}

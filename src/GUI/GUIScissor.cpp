// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIScissor.h"
#include "../Gumps/Gump.h"
#include "../Utility/PerfMarker.h"

CGUIScissor::CGUIScissor(bool enabled, int baseX, int baseY, int x, int y, int width, int height)
    : CGUIPolygonal(GOT_SCISSOR, x, y, width, height)
    , BaseX(baseX)
    , BaseY(baseY)
{
    Enabled = enabled;
}

CGUIScissor::~CGUIScissor()
{
}

void CGUIScissor::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);

    if (Enabled)
    {
        int x = m_X;
        int y = m_Y;

        if (GumpParent != nullptr)
        {
            x -= GumpParent->GumpRect.Position.X;
            y -= GumpParent->GumpRect.Position.Y;
        }
#ifndef NEW_RENDERER_ENABLED
        g_GL.PushScissor(BaseX + x, BaseY + y, Width, Height);
#else
        Render_PushScissor(BaseX + x, BaseY + y, Width, Height);
#endif
    }
    else
    {
#ifndef NEW_RENDERER_ENABLED
        g_GL.PopScissor();
#else
        Render_PopScissor();
#endif
    }
}

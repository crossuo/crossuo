// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GUIGlobalColor.h"
#include "../Renderer/RenderAPI.h"
#include "../Utility/PerfMarker.h"

CGUIGlobalColor::CGUIGlobalColor(bool enabled, float r, float g, float b, float a)
    : CBaseGUI(GOT_GLOBAL_COLOR, 0, 0, 0, 0, 0)
    , R(r)
    , G(g)
    , B(b)
    , A(a)
{
    Enabled = enabled;
}

CGUIGlobalColor::~CGUIGlobalColor()
{
}

void CGUIGlobalColor::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
    if (Enabled)
    {
#ifndef NEW_RENDERER_ENABLED
        glColor4f(R, G, B, A);
#else
        RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ { R, G, B, A } });
#endif
    }
}

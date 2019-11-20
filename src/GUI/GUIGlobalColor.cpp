// MIT License
// Copyright (C) December 2016 Hotride

#include "GUIGlobalColor.h"
#include "../Renderer/RenderAPI.h"
#include "../Utility/PerfMarker.h"

extern RenderCmdList *g_renderCmdList;

CGUIGlobalColor::CGUIGlobalColor(bool enabled, GLfloat r, GLfloat g, GLfloat b, GLfloat a)
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

// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIAlphaBlending.h"
#include "../Utility/PerfMarker.h"
#include "../Renderer/RenderAPI.h"

extern RenderCmdList *g_renderCmdList;

CGUIAlphaBlending::CGUIAlphaBlending(bool enabled, float alpha)
#ifndef NEW_RENDERER_ENABLED
    : CGUIBlending(enabled, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
#else
    : CBaseGUI(GOT_BLENDING, 0, 0, 0, 0, 0)
#endif
    , Alpha(alpha)
{
#ifdef NEW_RENDERER_ENABLED
    Enabled = enabled;
#endif
}

CGUIAlphaBlending::~CGUIAlphaBlending()
{
}

void CGUIAlphaBlending::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
    DEBUG_TRACE_FUNCTION;
#ifndef NEW_RENDERER_ENABLED
    CGUIBlending::Draw(checktrans);

    if (Enabled)
    {
        glColor4f(1.0f, 1.0f, 1.0f, Alpha);
    }
    else
    {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }
#else
    if (Enabled)
    {
        RenderAdd_SetBlend(
            g_renderCmdList,
            BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                           BlendFactor::BlendFactor_OneMinusSrcAlpha });
        RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ { 1.f, 1.f, 1.f, Alpha } });
    }
    else
    {
        RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ g_ColorWhite });
        RenderAdd_DisableBlend(g_renderCmdList);
    }
#endif
}

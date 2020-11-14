// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GUIAlphaBlending.h"
#include "../Utility/PerfMarker.h"
#include "../Renderer/RenderAPI.h"

CGUIAlphaBlending::CGUIAlphaBlending(bool enabled, float alpha)
    : CBaseGUI(GOT_BLENDING, 0, 0, 0, 0, 0)
    , Enabled(enabled)
    , Alpha(alpha)
{
}

void CGUIAlphaBlending::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
#ifndef NEW_RENDERER_ENABLED
    if (Enabled)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(1.0f, 1.0f, 1.0f, Alpha);
    }
    else
    {
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glDisable(GL_BLEND);
    }
#else
    if (Enabled)
    {
        RenderAdd_SetBlend(
            g_renderCmdList,
            BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                           BlendFactor::BlendFactor_OneMinusSrcAlpha });
        RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ { 1.f, 1.f, 1.f, Alpha } });
        // FIXME: bug ^^^ : glColor4f(1.0f, 1.0f, 1.0f, Alpha);
    }
    else
    {
        // FIXME: bug vvvv : glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ g_ColorWhite });
        RenderAdd_DisableBlend(g_renderCmdList);
    }
#endif
}

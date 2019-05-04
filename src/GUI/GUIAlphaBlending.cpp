// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIAlphaBlending.h"
#include "Renderer/RenderAPI.h"

extern RenderCmdList *g_renderCmdList;

CGUIAlphaBlending::CGUIAlphaBlending(bool enabled, float alpha)
    : CGUIBlending(enabled, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
    , Alpha(alpha)
{
}

CGUIAlphaBlending::~CGUIAlphaBlending()
{
}

void CGUIAlphaBlending::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    CGUIBlending::Draw(checktrans);

#ifndef NEW_RENDERER_ENABLED
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
        RenderAdd_SetColor(g_renderCmdList, &SetColorCmd({ 1.f, 1.f, 1.f, Alpha }));
    }
    else
    {
        RenderAdd_SetColor(g_renderCmdList, &SetColorCmd(g_ColorWhite));
    }
#endif
}

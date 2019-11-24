﻿// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIChecktrans.h"
#include "Renderer/RenderAPI.h"

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
    DEBUG_TRACE_FUNCTION;
#ifndef NEW_RENDERER_ENABLED
    glColorMask(0u, 0u, 0u, 0u);

    glStencilFunc(GL_ALWAYS, 1, 1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    g_GL.DrawPolygone(m_X, m_Y, Width, Height);

    glColorMask(1u, 1u, 1u, 1u);

    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilFunc(GL_NOTEQUAL, 1, 1);
#else
    RenderAdd_SetColorMask(g_renderCmdList, &SetColorMaskCmd(ColorMask::ColorMask_None));
    RenderAdd_SetStencil(
        g_renderCmdList,
        &StencilStateCmd(
            StencilFunc::StencilFunc_AlwaysPass,
            1,
            1,
            StencilOp::Keep,
            StencilOp::Keep,
            StencilOp::Replace));
    RenderAdd_DrawUntexturedQuad(g_renderCmdList, &DrawUntexturedQuadCmd(m_X, m_Y, Width, Height));
    RenderAdd_SetColorMask(g_renderCmdList, &SetColorMaskCmd(ColorMask::ColorMask_All));
    RenderAdd_SetStencil(
        g_renderCmdList,
        &StencilStateCmd(
            StencilFunc::StencilFunc_Different,
            1,
            1,
            StencilOp::Keep,
            StencilOp::Keep,
            StencilOp::Keep));
#endif
}

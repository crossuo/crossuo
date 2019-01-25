// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIChecktrans.h"

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
    glColorMask(0u, 0u, 0u, 0u);

    glStencilFunc(GL_ALWAYS, 1, 1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    g_GL.DrawPolygone(m_X, m_Y, Width, Height);

    glColorMask(1u, 1u, 1u, 1u);

    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilFunc(GL_NOTEQUAL, 1, 1);
}

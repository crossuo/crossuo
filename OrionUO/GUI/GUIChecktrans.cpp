// MIT License
// Copyright (C) August 2016 Hotride

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
    glColorMask(false, false, false, false);

    glStencilFunc(GL_ALWAYS, 1, 1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    g_GL.DrawPolygone(m_X, m_Y, Width, Height);

    glColorMask(true, true, true, true);

    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilFunc(GL_NOTEQUAL, 1, 1);
}


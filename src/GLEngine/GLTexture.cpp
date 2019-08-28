// MIT License
// Copyright (C) August 2016 Hotride

CGLTexture::~CGLTexture()
{
    DEBUG_TRACE_FUNCTION;
    Clear();
}

void CGLTexture::Draw(int x, int y, bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    if (Texture == 0)
        return;

    if (checktrans)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        g_GL_Draw(*this, x, y);
        glDisable(GL_BLEND);
        glEnable(GL_STENCIL_TEST);
        g_GL_Draw(*this, x, y);
        glDisable(GL_STENCIL_TEST);
    }
    else
    {
        g_GL_Draw(*this, x, y);
    }
}

void CGLTexture::Draw(int x, int y, int width, int height, bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    if (Texture == 0)
        return;

    if (width == 0)
    {
        width = Width;
    }

    if (height == 0)
    {
        height = Height;
    }

    if (checktrans)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        g_GL_DrawStretched(*this, x, y, width, height);
        glDisable(GL_BLEND);
        glEnable(GL_STENCIL_TEST);
        g_GL_DrawStretched(*this, x, y, width, height);
        glDisable(GL_STENCIL_TEST);
    }
    else
    {
        g_GL_DrawStretched(*this, x, y, width, height);
    }
}

void CGLTexture::Draw_Tooltip(int x, int y, int width, int height)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
    g_GL.DrawPolygone(x, y, width, height);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_BLEND);
    g_GL_Draw(*this, x + 6, y + 4);
}

void CGLTexture::DrawRotated(int x, int y, float angle)
{
    DEBUG_TRACE_FUNCTION;
    if (Texture != 0)
    {
        g_GL_DrawRotated(*this, x, y, angle);
    }
}

void CGLTexture::DrawTransparent(int x, int y, bool stencil)
{
    DEBUG_TRACE_FUNCTION;
    if (Texture != 0)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(1.0f, 1.0f, 1.0f, 0.25f);
        g_GL_Draw(*this, x, y);
        glDisable(GL_BLEND);

        if (stencil)
        {
            glEnable(GL_STENCIL_TEST);
            g_GL_Draw(*this, x, y);
            glDisable(GL_STENCIL_TEST);
        }
    }
}

void CGLTexture::Clear()
{
    DEBUG_TRACE_FUNCTION;
    if (Texture != 0)
    {
        glDeleteTextures(1, &Texture);
        Texture = 0;
    }

    if (VertexBuffer != 0)
    {
        glDeleteBuffers(1, &VertexBuffer);
        VertexBuffer = 0;
    }

    if (MirroredVertexBuffer != 0)
    {
        glDeleteBuffers(1, &MirroredVertexBuffer);
        MirroredVertexBuffer = 0;
    }
}

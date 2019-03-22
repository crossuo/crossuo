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

// FIXME
#include "../Managers/MouseManager.h"

uint16_t CGLHTMLTextTexture::WebLinkUnderMouse(int x, int y)
{
    DEBUG_TRACE_FUNCTION;
    x = g_MouseManager.Position.X - x;
    y = g_MouseManager.Position.Y - y;

    for (auto it = m_WebLinkRect.begin(); it != m_WebLinkRect.end(); ++it)
    {
        if (y >= (*it).StartY && y < (*it).StartY + (*it).EndY)
        {
            if (x >= (*it).StartX && x < (*it).StartX + (*it).EndX)
            {
                return it->LinkID;
            }
        }
    }

    return 0;
}

// MIT License
// Copyright (C) August 2016 Hotride

#include "../Managers/MouseManager.h"

CGLTexture::CGLTexture()
{
}

CGLTexture::~CGLTexture()
{
    DEBUG_TRACE_FUNCTION;
    Clear();
}

void CGLTexture::Draw(int x, int y, bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    if (Texture != 0)
    {
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
}

void CGLTexture::Draw(int x, int y, int width, int height, bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    if (Texture != 0)
    {
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
    Width = 0;
    Height = 0;
    m_HitMap.clear();

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

bool CGLTexture::Select(int x, int y, bool pixelCheck)
{
    x = g_MouseManager.Position.X - x;
    y = g_MouseManager.Position.Y - y;
    return TestHit(x, y, pixelCheck);
}

bool CGLTexture::TestHit(int x, int y, bool pixelCheck)
{
    if (x >= 0 && y >= 0 && x < Width && y < Height)
    {
        if (!pixelCheck)
        {
            return true;
        }

        int pos = (y * Width) + x;
        if (pos < (int)m_HitMap.size())
        {
            return m_HitMap[pos];
        }
    }

    return false;
}

template <typename T>
void CGLTexture::BuildHitMask(int w, int h, T *pixels)
{
    m_HitMap.resize(w * h);
    int pos = 0;
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            m_HitMap[pos] = (pixels[pos] != 0);
            pos++;
        }
    }
}

template void CGLTexture::BuildHitMask<uint16_t>(int w, int h, uint16_t *pixels);
template void CGLTexture::BuildHitMask<uint32_t>(int w, int h, uint32_t *pixels);

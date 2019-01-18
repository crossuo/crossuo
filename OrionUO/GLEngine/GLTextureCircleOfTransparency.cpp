// MIT License
// Copyright (C) August 2016 Hotride

#include "../Managers/ConfigManager.h"

CGLTextureCircleOfTransparency g_CircleOfTransparency;

CGLTextureCircleOfTransparency::CGLTextureCircleOfTransparency()
{
}

CGLTextureCircleOfTransparency::~CGLTextureCircleOfTransparency()
{
    DEBUG_TRACE_FUNCTION;
    Clear();
}

void CGLTextureCircleOfTransparency::CreatePixels(
    int radius, short &width, short &height, vector<uint32_t> &pixels)
{
    DEBUG_TRACE_FUNCTION;
    int fixRadius = radius + 1;
    int mulRadius = fixRadius * 2;

    pixels.resize(mulRadius * mulRadius);

    width = mulRadius;
    height = mulRadius;

    for (int x = -fixRadius; x < fixRadius; x++)
    {
        intptr_t mulX = x * x;
        int posX = (((int)x + fixRadius) * mulRadius) + fixRadius;

        for (int y = -fixRadius; y < fixRadius; y++)
        {
            int r = (int)sqrt(mulX + (y * y));
            uint8_t pic = ((r <= radius) ? ((radius - r) & 0xFF) : 0);

            int pos = posX + (int)y;

            pixels[pos] = pic;
        }
    }
}

bool CGLTextureCircleOfTransparency::Create(int radius)
{
    DEBUG_TRACE_FUNCTION;
    if (radius <= 0)
    {
        return false;
    }

    if (radius > 200)
    {
        radius = 200;
    }

    if (radius == Radius)
    {
        return true;
    }

    vector<uint32_t> pixels;
    CreatePixels(radius, Width, Height, pixels);

    Radius = radius;
    if (Texture != 0)
    {
        glDeleteTextures(1, &Texture);
        Texture = 0;
    }
    g_GL_BindTexture32(*this, Width, Height, &pixels[0]);
    return true;
}

void CGLTextureCircleOfTransparency::Draw(int x, int y, bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    if (Texture != 0)
    {
        X = x - Width / 2;
        Y = y - Height / 2;

        glEnable(GL_STENCIL_TEST);

        glColorMask(0u, 0u, 0u, 1u);

        glStencilFunc(GL_ALWAYS, 1, 1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

        g_GL_Draw(*this, X, Y);

        glColorMask(1u, 1u, 1u, 1u);

        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilFunc(GL_NOTEQUAL, 1, 1);

        glDisable(GL_STENCIL_TEST);
    }
}

void CGLTextureCircleOfTransparency::Redraw()
{
    DEBUG_TRACE_FUNCTION;
    glClear(GL_STENCIL_BUFFER_BIT);

    if (g_ConfigManager.UseCircleTrans && Texture != 0)
    {
        glEnable(GL_STENCIL_TEST);

        glColorMask(0u, 0u, 0u, 1u);

        glStencilFunc(GL_ALWAYS, 1, 1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

        g_GL_Draw(*this, X, Y);

        glColorMask(1u, 1u, 1u, 1u);

        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilFunc(GL_NOTEQUAL, 1, 1);

        glDisable(GL_STENCIL_TEST);
    }
}

// MIT License
// Copyright (C) August 2016 Hotride

#include "../Managers/ConfigManager.h"
#include "Renderer/RenderAPI.h"
#include "Utility/PerfMarker.h"

extern RenderCmdList *g_renderCmdList;

CGLTextureCircleOfTransparency g_CircleOfTransparency;

std::vector<uint32_t> CreateCircleSprite(int radius, int16_t &width, int16_t &height)
{
    DEBUG_TRACE_FUNCTION;
    int fixRadius = radius + 1;
    int mulRadius = fixRadius * 2;
    std::vector<uint32_t> pixels;
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
    return pixels;
}

CGLTextureCircleOfTransparency::~CGLTextureCircleOfTransparency()
{
    DEBUG_TRACE_FUNCTION;
    m_Sprite.Clear();
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

    int16_t w = 0, h = 0;
    auto pixels = CreateCircleSprite(radius, w, h);
    Radius = radius;
    m_Sprite.Clear();
    m_Sprite.LoadSprite32(w, h, pixels.data());
    return true;
}

void CGLTextureCircleOfTransparency::Draw(int x, int y, bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
    DEBUG_TRACE_FUNCTION;
    if (m_Sprite.Texture == nullptr)
    {
        return;
    }

    X = x - m_Sprite.Width / 2;
    Y = y - m_Sprite.Height / 2;

#ifndef NEW_RENDERER_ENABLED
    glEnable(GL_STENCIL_TEST);
    glColorMask(0u, 0u, 0u, 1u);
    glStencilFunc(GL_ALWAYS, 1, 1);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    g_GL.Draw(*m_Sprite.Texture, X, Y);
    glColorMask(1u, 1u, 1u, 1u);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilFunc(GL_NOTEQUAL, 1, 1);
    glDisable(GL_STENCIL_TEST);
#else
    auto stencilCmd = StencilStateCmd{ StencilFunc::StencilFunc_AlwaysPass,
                                       StencilOp::StencilOp_Keep,
                                       StencilOp::StencilOp_Keep,
                                       StencilOp::StencilOp_Replace,
                                       1,
                                       1 };
    RenderAdd_SetStencil(g_renderCmdList, stencilCmd);

    RenderAdd_SetColorMask(g_renderCmdList, SetColorMaskCmd{ ColorMask::ColorMask_Alpha });
    RenderAdd_DrawQuad(
        g_renderCmdList,
        DrawQuadCmd{
            m_Sprite.Texture->Texture, X, Y, m_Sprite.Texture->Width, m_Sprite.Texture->Height });
    RenderAdd_SetColorMask(g_renderCmdList, SetColorMaskCmd{ ColorMask::ColorMask_All });

    // TODO skipping some redundant state changes due to stencil being disabled
    // test this is still working as intended
    RenderAdd_DisableStencil(g_renderCmdList);
#endif
}

void CGLTextureCircleOfTransparency::Redraw()
{
    ScopedPerfMarker(__FUNCTION__);
    DEBUG_TRACE_FUNCTION;
#ifndef NEW_RENDERER_ENABLED
    glClear(GL_STENCIL_BUFFER_BIT);
    if (g_ConfigManager.UseCircleTrans && m_Sprite.Texture != nullptr)
    {
        glEnable(GL_STENCIL_TEST);
        glColorMask(0u, 0u, 0u, 1u);
        glStencilFunc(GL_ALWAYS, 1, 1);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        g_GL.Draw(*m_Sprite.Texture, X, Y);
        glColorMask(1u, 1u, 1u, 1u);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilFunc(GL_NOTEQUAL, 1, 1);
        glDisable(GL_STENCIL_TEST);
    }
#else
    RenderAdd_ClearRT(g_renderCmdList, ClearRTCmd{ ClearRT::ClearRT_Stencil });
    if (g_ConfigManager.UseCircleTrans && m_Sprite.Texture != nullptr)
    {
        RenderAdd_SetColorMask(g_renderCmdList, SetColorMaskCmd{ ColorMask::ColorMask_Alpha });
        auto stencilCmd = StencilStateCmd{ StencilFunc::StencilFunc_AlwaysPass,
                                           StencilOp::StencilOp_Keep,
                                           StencilOp::StencilOp_Keep,
                                           StencilOp::StencilOp_Replace,
                                           1,
                                           1 };
        RenderAdd_SetStencil(g_renderCmdList, stencilCmd);

        RenderAdd_DrawQuad(
            g_renderCmdList,
            DrawQuadCmd{ m_Sprite.Texture->Texture,
                         X,
                         Y,
                         m_Sprite.Texture->Width,
                         m_Sprite.Texture->Height });

        RenderAdd_SetColorMask(g_renderCmdList, SetColorMaskCmd{ ColorMask::ColorMask_All });

        // TODO skipping some redundant state changes due to stencil being disabled
        // test this is still working as intended
        RenderAdd_DisableStencil(g_renderCmdList);
    }
#endif
}

// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include <external/gfx/gfx.h>
#include "GLTexture.h"
#include "../Renderer/RenderAPI.h"
#include "../Utility/PerfMarker.h"

CGLTexture::~CGLTexture()
{
    Clear();
}

void CGLTexture::Draw(int x, int y, bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
#ifndef NEW_RENDERER_ENABLED
    if (Texture == 0)
        return;

    if (checktrans)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        g_GL.Draw(*this, x, y);
        glDisable(GL_BLEND);
        glEnable(GL_STENCIL_TEST);
        g_GL.Draw(*this, x, y);
        glDisable(GL_STENCIL_TEST);
    }
    else
    {
        g_GL.Draw(*this, x, y);
    }
#else
    if (Texture == RENDER_TEXTUREHANDLE_INVALID)
        return;

    auto cmd = DrawQuadCmd{ Texture, x, y, Width, Height };
    static const bool trace = getenv("XUO_GL1_TRACE") != nullptr;
    if (trace)
    {
        Warning(Renderer, "TEXDRAW tex=%u at=(%d,%d) size=(%ux%u) checktrans=%d", Texture, x, y,
                 Width, Height, (int)checktrans);
    }
    if (checktrans)
    {
        SCOPED_GL_DEBUG_MARKER_LABEL("Object with Transparency");
        RenderAdd_SetBlend(
            g_renderCmdList,
            BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                           BlendFactor::BlendFactor_OneMinusSrcAlpha });
        RenderAdd_DrawQuad(g_renderCmdList, cmd);
        RenderAdd_DisableBlend(g_renderCmdList);

        // Use NOTEQUAL stencil test with ref=1 to match GL1 behavior
        // Objects INSIDE circle (stencil=1): NOTEQUAL(1,1) = false, don't draw (stay blended)
        // Objects OUTSIDE circle (stencil=0): NOTEQUAL(0,1) = true, draw full opacity
        RenderAdd_SetStencil(
            g_renderCmdList,
            StencilStateCmd{ StencilFunc::StencilFunc_Different,
                             StencilOp::StencilOp_Keep,
                             StencilOp::StencilOp_Keep,
                             StencilOp::StencilOp_Keep,
                             1,
                             1 });
        RenderAdd_EnableStencil(g_renderCmdList);
        RenderAdd_DrawQuad(g_renderCmdList, cmd);
        RenderAdd_DisableStencil(g_renderCmdList);
    }
    else
    {
        RenderAdd_DrawQuad(g_renderCmdList, cmd);
    }
#endif
}

void CGLTexture::Draw(int x, int y, int width, int height, bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
#ifndef NEW_RENDERER_ENABLED
    if (Texture == 0)
#else
    if (Texture == RENDER_TEXTUREHANDLE_INVALID)
#endif
    {
        return;
    }

    if (width == 0)
    {
        width = Width;
    }

    if (height == 0)
    {
        height = Height;
    }

#ifndef NEW_RENDERER_ENABLED
    if (checktrans)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        g_GL.DrawStretched(*this, x, y, width, height);
        glDisable(GL_BLEND);
        glEnable(GL_STENCIL_TEST);
        g_GL.DrawStretched(*this, x, y, width, height);
        glDisable(GL_STENCIL_TEST);
    }
    else
    {
        g_GL.DrawStretched(*this, x, y, width, height);
    }
#else
    auto cmd = DrawQuadCmd{ Texture,
                            x,
                            y,
                            uint32_t(width),
                            uint32_t(height),
                            width / float(Width),
                            height / float(Height) };

    if (checktrans)
    {
        SCOPED_GL_DEBUG_MARKER_LABEL("Object With Transparency 2");
        RenderAdd_SetBlend(
            g_renderCmdList,
            BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                           BlendFactor::BlendFactor_OneMinusSrcAlpha });
        RenderAdd_DrawQuad(g_renderCmdList, cmd);
        RenderAdd_DisableBlend(g_renderCmdList);

        // Use NOTEQUAL stencil test with ref=1 to match GL1 behavior
        // Objects INSIDE circle (stencil=1): NOTEQUAL(1,1) = false, don't draw (stay blended)
        // Objects OUTSIDE circle (stencil=0): NOTEQUAL(0,1) = true, draw full opacity
        RenderAdd_SetStencil(
            g_renderCmdList,
            StencilStateCmd{ StencilFunc::StencilFunc_Different,
                             StencilOp::StencilOp_Keep,
                             StencilOp::StencilOp_Keep,
                             StencilOp::StencilOp_Keep,
                             1,
                             1 });
        RenderAdd_EnableStencil(g_renderCmdList);
        RenderAdd_DrawQuad(g_renderCmdList, cmd);
        RenderAdd_DisableStencil(g_renderCmdList);
    }
    else
    {
        RenderAdd_DrawQuad(g_renderCmdList, cmd);
    }
#endif
}

void CGLTexture::Draw_Tooltip(int x, int y, int width, int height)
{
    SCOPED_GL_DEBUG_MARKER_LABEL("CGLTexture::Draw_Tooltip");
#ifndef NEW_RENDERER_ENABLED
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
    g_GL.DrawPolygone(x, y, width, height);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_BLEND);
    g_GL.Draw(*this, x + 6, y + 4);
#else
    RenderAdd_SetBlend(
        g_renderCmdList,
        BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
        BlendFactor::BlendFactor_OneMinusSrcAlpha });
    RenderAdd_DrawUntexturedQuad(
        g_renderCmdList,
        DrawUntexturedQuadCmd{ x, y, uint32_t(width), uint32_t(height), { 0.f, 0.f, 0.f, 0.5f } });
    RenderAdd_DisableBlend(g_renderCmdList);
    RenderAdd_DrawQuad(
        g_renderCmdList,
        DrawQuadCmd{ Texture, x + 6, y + 4, Width, Height });
#endif
}

void CGLTexture::DrawRotated(int x, int y, float angle)
{
#ifndef NEW_RENDERER_ENABLED
    if (Texture == 0)
    {
        return;
    }

    g_GL.DrawRotated(*this, x, y, angle);
#else
    if (Texture == RENDER_TEXTUREHANDLE_INVALID)
    {
        return;
    }

    auto cmd = DrawRotatedQuadCmd{ Texture, x, y - Height, Width, Height, angle };
    RenderAdd_DrawRotatedQuad(g_renderCmdList, cmd);
#endif
}

void CGLTexture::DrawTransparent(int x, int y, bool stencil)
{
#ifndef NEW_RENDERER_ENABLED
    if (Texture == 0)
    {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 0.25f);
    g_GL.Draw(*this, x, y);
    glDisable(GL_BLEND);

    if (stencil)
    {
        glEnable(GL_STENCIL_TEST);
        g_GL.Draw(*this, x, y);
        glDisable(GL_STENCIL_TEST);
    }
#else
    if (Texture == RENDER_TEXTUREHANDLE_INVALID)
    {
        return;
    }

    auto cmd = DrawQuadCmd{ Texture, x, y, Width, Height, 1.f, 1.f, { 1.f, 1.f, 1.f, 0.25f } };
    RenderAdd_SetBlend(
        g_renderCmdList,
        BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                       BlendFactor::BlendFactor_OneMinusSrcAlpha });
    RenderAdd_DrawQuad(g_renderCmdList, cmd);
    RenderAdd_DisableBlend(g_renderCmdList);

    if (stencil)
    {
        // Use NOTEQUAL stencil test with ref=1 to match GL1 behavior
        // Objects INSIDE circle (stencil=1): NOTEQUAL(1,1) = false, don't draw (stay at 0.25 alpha)
        // Objects OUTSIDE circle (stencil=0): NOTEQUAL(0,1) = true, draw full opacity
        RenderAdd_SetStencil(
            g_renderCmdList,
            StencilStateCmd{ StencilFunc::StencilFunc_Different,
                             StencilOp::StencilOp_Keep,
                             StencilOp::StencilOp_Keep,
                             StencilOp::StencilOp_Keep,
                             1,
                             1 });
       RenderAdd_EnableStencil(g_renderCmdList);
       RenderAdd_DrawQuad(g_renderCmdList, cmd);
       RenderAdd_DisableStencil(g_renderCmdList);
    }
#endif
}

void CGLTexture::Clear()
{
#ifndef NEW_RENDERER_ENABLED
    if (Texture != 0)
    {
        glDeleteTextures(1, &Texture);
        Texture = 0;
    }
#else
    if (Texture != RENDER_TEXTUREHANDLE_INVALID)
    {
        Render_DestroyTexture(Texture);
        Texture = RENDER_TEXTUREHANDLE_INVALID;
    }
#endif
}

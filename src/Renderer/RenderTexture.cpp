// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include <cstdlib>
#include <xuocore/enumlist.h>
#include "RenderTexture.h"
#include "RenderAPI.h"
#include "../Utility/PerfMarker.h"
#include "common/logging/logging.h"

CRenderTexture::~CRenderTexture()
{
    Clear();
}

void CRenderTexture::Draw(int x, int y, bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
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
        // single draw: blending is enabled and the shader forces alpha to
        // 1.0 outside the circle of transparency (previously a blended pass
        // plus a stencil-tested opaque pass)
        const int baseMode = Render_GetCurrentDrawMode();
        RenderAdd_SetDrawMode(
            g_renderCmdList, SetDrawModeCmd{ baseMode + SDM_CHECKTRANS_BASE });
        RenderAdd_SetBlend(
            g_renderCmdList,
            BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                           BlendFactor::BlendFactor_OneMinusSrcAlpha });
        RenderAdd_DrawQuad(g_renderCmdList, cmd);
        RenderAdd_SetDrawMode(g_renderCmdList, SetDrawModeCmd{ baseMode });
        RenderAdd_DisableBlend(g_renderCmdList);
    }
    else
    {
        RenderAdd_DrawQuad(g_renderCmdList, cmd);
    }
}

void CRenderTexture::Draw(int x, int y, int width, int height, bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
    if (Texture == RENDER_TEXTUREHANDLE_INVALID)
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
        // single draw: blending is enabled and the shader forces alpha to
        // 1.0 outside the circle of transparency (previously a blended pass
        // plus a stencil-tested opaque pass)
        const int baseMode = Render_GetCurrentDrawMode();
        RenderAdd_SetDrawMode(
            g_renderCmdList, SetDrawModeCmd{ baseMode + SDM_CHECKTRANS_BASE });
        RenderAdd_SetBlend(
            g_renderCmdList,
            BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                           BlendFactor::BlendFactor_OneMinusSrcAlpha });
        RenderAdd_DrawQuad(g_renderCmdList, cmd);
        RenderAdd_SetDrawMode(g_renderCmdList, SetDrawModeCmd{ baseMode });
        RenderAdd_DisableBlend(g_renderCmdList);
    }
    else
    {
        RenderAdd_DrawQuad(g_renderCmdList, cmd);
    }
}

void CRenderTexture::Draw_Tooltip(int x, int y, int width, int height)
{
    SCOPED_GL_DEBUG_MARKER_LABEL("CRenderTexture::Draw_Tooltip");
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
}

void CRenderTexture::DrawRotated(int x, int y, float angle)
{
    if (Texture == RENDER_TEXTUREHANDLE_INVALID)
    {
        return;
    }

    auto cmd = DrawRotatedQuadCmd{ Texture, x, y - Height, Width, Height, angle };
    RenderAdd_DrawRotatedQuad(g_renderCmdList, cmd);
}

void CRenderTexture::DrawTransparent(int x, int y, bool stencil)
{
    if (Texture == RENDER_TEXTUREHANDLE_INVALID)
    {
        return;
    }

    auto cmd = DrawQuadCmd{ Texture, x, y, Width, Height, 1.f, 1.f, { 1.f, 1.f, 1.f, 0.25f } };
    RenderAdd_SetBlend(
        g_renderCmdList,
        BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                       BlendFactor::BlendFactor_OneMinusSrcAlpha });

    if (stencil)
    {
        // single draw: blending stays enabled and the shader forces alpha to
        // 1.0 outside the circle of transparency (previously a blended pass at
        // 0.25 alpha plus a stencil-tested pass at full opacity)
        const int baseMode = Render_GetCurrentDrawMode();
        RenderAdd_SetDrawMode(
            g_renderCmdList, SetDrawModeCmd{ baseMode + SDM_CHECKTRANS_BASE });
        RenderAdd_DrawQuad(g_renderCmdList, cmd);
        RenderAdd_SetDrawMode(g_renderCmdList, SetDrawModeCmd{ baseMode });
    }
    else
    {
        RenderAdd_DrawQuad(g_renderCmdList, cmd);
    }

    RenderAdd_DisableBlend(g_renderCmdList);
}

void CRenderTexture::Clear()
{
    // the array placement is a property of the texture object: once the
    // texture handle is gone the sprite must be re-placed (the array layer
    // still holds the previous pixels, and the stale layer index would keep
    // a recycled name sampling it)
#if defined(USE_GL3) || defined(USE_GLES)
    if (ArrayLayer >= 0)
    {
        // hand the layer back so the next placement reuses it instead of
        // growing the array (release before resetting the cache)
        Render_ArtArrayRelease(ArrayLayer);
    }
#endif
    ArrayLayer = -1;
    ArtU = 1.0f;
    ArtV = 1.0f;
    if (Texture != RENDER_TEXTUREHANDLE_INVALID)
    {
        Render_DestroyTexture(Texture);
        Texture = RENDER_TEXTUREHANDLE_INVALID;
    }
}

int CRenderTexture::ArtArrayPlace()
{
#if defined(USE_GL3) || defined(USE_GLES)
    if (ArrayLayer < 0)
    {
        const auto place = Render_ArtArrayPlace(Texture, Width, Height);
        ArrayLayer = place.layer;
        ArtU = place.u;
        ArtV = place.v;
    }
    return ArrayLayer;
#else
    return -1;
#endif
}

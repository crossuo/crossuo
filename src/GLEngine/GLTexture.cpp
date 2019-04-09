// MIT License
// Copyright (C) August 2016 Hotride

#include "Renderer/RenderAPI.h"
extern RenderCmdList *g_renderCmdList;

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

    auto textureCmd = RenderAdd_TextureCmd(Texture, x, y, Width, Height, 1.f, 1.f);

    if (checktrans)
    {
        RenderAdd_SetBlend(g_renderCmdList, &RenderAdd_Blend(BlendFunc::SrcAlpha_OneMinusSrcAlpha));
        RenderAdd_Texture(g_renderCmdList, &textureCmd, 1);
        RenderAdd_DisableBlend(g_renderCmdList);

        RenderAdd_SetStencil(g_renderCmdList, &RenderAdd_Stencil());
        RenderAdd_Texture(g_renderCmdList, &textureCmd, 1);
        RenderAdd_DisableStencil(g_renderCmdList);
    }
    else
    {
        RenderAdd_Texture(g_renderCmdList, &textureCmd, 1);
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

    auto textureCmd = RenderAdd_TextureCmd(
        Texture, x, y, width, height, width / float(Width), height / float(Height));

    if (checktrans)
    {
        RenderAdd_SetBlend(g_renderCmdList, &RenderAdd_Blend(BlendFunc::SrcAlpha_OneMinusSrcAlpha));
        RenderAdd_Texture(g_renderCmdList, &textureCmd, 1);
        RenderAdd_DisableBlend(g_renderCmdList);

        RenderAdd_SetStencil(g_renderCmdList, &RenderAdd_Stencil());
        RenderAdd_Texture(g_renderCmdList, &textureCmd, 1);
        RenderAdd_DisableStencil(g_renderCmdList);
    }
    else
    {
        RenderAdd_Texture(g_renderCmdList, &textureCmd, 1);
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
    if (Texture == 0)
    {
        return;
    }

    auto textureCmd = RenderAdd_RotatedTextureCmd(Texture, x, y - Height, Width, Height, angle);

    RenderAdd_RotatedTexture(g_renderCmdList, &textureCmd, 1);
}

void CGLTexture::DrawTransparent(int x, int y, bool stencil)
{
    DEBUG_TRACE_FUNCTION;
    if (Texture != 0)
    {
        auto textureCmd = RenderAdd_TextureCmd(
            Texture, x, y, Width, Height, 1.f, 1.f, { { 1.f, 1.f, 1.f, 0.25f } });
        RenderAdd_SetBlend(g_renderCmdList, &RenderAdd_Blend(BlendFunc::SrcAlpha_OneMinusSrcAlpha));
        RenderAdd_Texture(g_renderCmdList, &textureCmd, 1);
        RenderAdd_DisableBlend(g_renderCmdList);

        if (stencil)
        {
            RenderAdd_SetStencil(g_renderCmdList, &RenderAdd_Stencil());
            RenderAdd_Texture(g_renderCmdList, &textureCmd, 1);
            RenderAdd_DisableStencil(g_renderCmdList);
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

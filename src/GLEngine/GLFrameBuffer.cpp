// MIT License
// Copyright (C) August 2016 Hotride

#include "Backend.h"
#include "Renderer/RenderAPI.h"
#include "Utility/PerfMarker.h"

#ifdef NEW_RENDERER_ENABLED
extern RenderCmdList *g_renderCmdList;
frame_buffer_t CGLFrameBuffer::m_OldFrameBuffer;
#endif

CGLFrameBuffer::CGLFrameBuffer()
{
}

CGLFrameBuffer::~CGLFrameBuffer()
{
    Free();
}

bool CGLFrameBuffer::Init(int width, int height)
{
    ScopedPerfMarker(__FUNCTION__);
    Free();

    bool result = false;

    if (width > 0 && height > 0)
    {
#ifndef NEW_RENDERER_ENABLED
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glGenTextures(1, &Texture.Texture);
        glBindTexture(GL_TEXTURE_2D, Texture.Texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            width,
            height,
            0,
            GL_BGRA,
            GL_UNSIGNED_INT_8_8_8_8,
            nullptr);

        GLint currentFrameBuffer = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFrameBuffer);

        glGenFramebuffers(1, &m_FrameBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FrameBuffer);

        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Texture.Texture, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
        {
            Texture.Width = width;
            Texture.Height = height;

            result = true;
            m_Ready = true;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, currentFrameBuffer);
#else
        m_FrameBuffer = Render_CreateFrameBuffer(uint32_t(width), uint32_t(height));
        result = m_FrameBuffer.texture != RENDER_TEXTUREHANDLE_INVALID &&
                 m_FrameBuffer.handle != RENDER_FRAMEBUFFER_INVALID;
        m_Ready = result;
        if (result)
        {
            m_Width = uint32_t(width);
            m_Height = uint32_t(height);
        }
#endif
    }

    return result;
}

void CGLFrameBuffer::Free()
{
    ScopedPerfMarker(__FUNCTION__);

#ifndef NEW_RENDERER_ENABLED
    Texture.Clear();

    if (m_FrameBuffer != 0)
    {
        glDeleteFramebuffers(1, &m_FrameBuffer);
        m_FrameBuffer = 0;
    }
#else
    if (m_FrameBuffer.handle != RENDER_FRAMEBUFFER_INVALID)
    {
        Render_DestroyFrameBuffer(m_FrameBuffer);
    }

    m_FrameBuffer = {};
#endif

    m_OldFrameBuffer = {};
}

void CGLFrameBuffer::Release()
{
    ScopedPerfMarker(__FUNCTION__);

#ifndef NEW_RENDERER_ENABLED
    glBindFramebuffer(GL_FRAMEBUFFER, m_OldFrameBuffer);

    glBindTexture(GL_TEXTURE_2D, Texture.Texture);
    glGenerateMipmap(GL_TEXTURE_2D);

    g_GL.RestorePort();
#else
    // TODO this isn't the place to keep track of the previous frame buffer
    RenderAdd_SetFrameBuffer(g_renderCmdList, &SetFrameBufferCmd{ m_OldFrameBuffer });

    RenderAdd_SetTexture(
        g_renderCmdList,
        &SetTextureCmd{ m_FrameBuffer.texture, RenderTextureType::Texture2D_Mipmapped });

    auto window_width = g_GameWindow.GetSize().Width;
    auto window_height = g_GameWindow.GetSize().Height;
    auto viewParams = SetViewParamsCmd{
        0, 0, window_width, window_height, window_width, window_height, -150, 150
    };
    RenderAdd_SetViewParams(g_renderCmdList, &viewParams);
#endif
}

bool CGLFrameBuffer::Ready(int width, int height)
{
    assert(width >= 0 && height >= 0);
#ifndef NEW_RENDERER_ENABLED
    return (m_Ready && Texture.Width == width && Texture.Height == height);
#else
    return (m_Ready && m_Width == uint32_t(width) && m_Height == uint32_t(height));
#endif
}

bool CGLFrameBuffer::ReadyMinSize(int width, int height)
{
    assert(width >= 0 && height >= 0);
#ifndef NEW_RENDERER_ENABLED
    return (m_Ready && Texture.Width >= width && Texture.Height >= height);
#else
    return (m_Ready && m_Width >= uint32_t(width) && m_Height >= uint32_t(height));
#endif
}

bool CGLFrameBuffer::Use()
{
    ScopedPerfMarker(__FUNCTION__);
    bool result = false;

    if (m_Ready)
    {
#ifndef NEW_RENDERER_ENABLED
        glEnable(GL_TEXTURE_2D);

        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_OldFrameBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FrameBuffer);
        glBindTexture(GL_TEXTURE_2D, Texture.Texture);

        glViewport(0, 0, Texture.Width, Texture.Height);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        glOrtho(0.0, Texture.Width, 0.0, Texture.Height, -150.0, 150.0);

        glMatrixMode(GL_MODELVIEW);
#else
        HACKRender_GetFrameBuffer(g_renderCmdList, &m_OldFrameBuffer);
        RenderAdd_SetFrameBuffer(g_renderCmdList, &SetFrameBufferCmd{ m_FrameBuffer });
        static constexpr bool flipped_y = true;
        RenderAdd_SetViewParams(
            g_renderCmdList,
            &SetViewParamsCmd{ 0,
                               0,
                               int(m_Width),
                               int(m_Height),
                               int(m_Width),
                               int(m_Height),
                               -150,
                               150,
                               1.f,
                               flipped_y });
#endif

        result = true;
    }

    return result;
}

void CGLFrameBuffer::Draw(int x, int y)
{
    ScopedPerfMarker(__FUNCTION__);
    if (m_Ready)
    {
        g_GL.OldTexture = 0;
#ifndef NEW_RENDERER_ENABLED
        g_GL.Draw(Texture, x, y);
#else
        auto cmd = DrawQuadCmd(m_FrameBuffer.texture, x, y, m_Width, m_Height);
        RenderAdd_DrawQuad(g_renderCmdList, &cmd, 1);
#endif
    }
}

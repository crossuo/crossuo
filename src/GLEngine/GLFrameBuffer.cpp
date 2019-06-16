// MIT License
// Copyright (C) August 2016 Hotride

#include "Backend.h"
#include "Renderer/RenderAPI.h"

extern RenderCmdList *g_renderCmdList;

CGLFrameBuffer::CGLFrameBuffer()
{
}

CGLFrameBuffer::~CGLFrameBuffer()
{
    Free();
}

bool CGLFrameBuffer::Init(int width, int height)
{
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
#endif
    }

    return result;
}

void CGLFrameBuffer::Free()
{
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
#ifndef NEW_RENDERER_ENABLED
    glBindFramebuffer(GL_FRAMEBUFFER, m_OldFrameBuffer);

    glBindTexture(GL_TEXTURE_2D, Texture.Texture);
    glGenerateMipmap(GL_TEXTURE_2D);

    g_GL.RestorePort();
#else
    RenderAdd_SetFrameBuffer(g_renderCmdList, SetFrameBufferCmd{ m_OldFrameBuffer });
    // TODO this isn't the place to keep track of the previous frame buffer
    // TODO who should know when to call glGenerateMipmap?
    // maybe this is a new specific command while this isn't replaced
    auto viewParams = RenderViewParams{ .viewport = { g_GameWindow.GetSize().Width,
                                                      g_GameWindow.GetSize().Height } };
    Render_SetViewParams(&viewParams);
    __debugbreak();
    continuar daqui substituindo o resto do arquivo e verificar o outro continuar daqui
#endif
}

bool CGLFrameBuffer::Ready(int width, int height)
{
    return (m_Ready && Texture.Width == width && Texture.Height == height);
}

bool CGLFrameBuffer::ReadyMinSize(int width, int height)
{
    return (m_Ready && Texture.Width >= width && Texture.Height >= height);
}

bool CGLFrameBuffer::Use()
{
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
#endif

        result = true;
    }

    return result;
}

void CGLFrameBuffer::Draw(int x, int y)
{
    if (m_Ready)
    {
        g_GL.OldTexture = 0;
#ifndef NEW_RENDERER_ENABLED
        g_GL.GL1_Draw(Texture, x, y);
#else
        auto cmd = DrawQuadCmd(Texture.Texture, x, y, Texture.Width, Texture.Height);
        RenderAdd_DrawQuad(g_renderCmdList, &cmd, 1);
#endif
    }
}

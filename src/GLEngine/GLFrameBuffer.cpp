// MIT License
// Copyright (C) August 2016 Hotride

CGLFrameBuffer::CGLFrameBuffer()
{
    DEBUG_TRACE_FUNCTION;
}

CGLFrameBuffer::~CGLFrameBuffer()
{
    DEBUG_TRACE_FUNCTION;
    Free();
}

bool CGLFrameBuffer::Init(int width, int height)
{
    DEBUG_TRACE_FUNCTION;
    Free();

    bool result = false;

    if (g_GL.CanUseFrameBuffer && (width != 0) && (height != 0))
    {
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
    }

    return result;
}

void CGLFrameBuffer::Free()
{
    DEBUG_TRACE_FUNCTION;
    Texture.Clear();

    if (g_GL.CanUseFrameBuffer && m_FrameBuffer != 0)
    {
        glDeleteFramebuffers(1, &m_FrameBuffer);
        m_FrameBuffer = 0;
    }

    m_OldFrameBuffer = 0;
}

void CGLFrameBuffer::Release()
{
    DEBUG_TRACE_FUNCTION;
    if (g_GL.CanUseFrameBuffer)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_OldFrameBuffer);

        glBindTexture(GL_TEXTURE_2D, Texture.Texture);
        glGenerateMipmap(GL_TEXTURE_2D);

        g_GL.RestorePort();
    }
}

bool CGLFrameBuffer::Ready(int width, int height)
{
    DEBUG_TRACE_FUNCTION;
    return (
        g_GL.CanUseFrameBuffer && m_Ready && Texture.Width == width && Texture.Height == height);
}

bool CGLFrameBuffer::ReadyMinSize(int width, int height)
{
    DEBUG_TRACE_FUNCTION;
    return (
        g_GL.CanUseFrameBuffer && m_Ready && Texture.Width >= width && Texture.Height >= height);
}

bool CGLFrameBuffer::Use()
{
    DEBUG_TRACE_FUNCTION;
    bool result = false;

    if (g_GL.CanUseFrameBuffer && m_Ready)
    {
        glEnable(GL_TEXTURE_2D);

        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_OldFrameBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_FrameBuffer);
        glBindTexture(GL_TEXTURE_2D, Texture.Texture);

        glViewport(0, 0, Texture.Width, Texture.Height);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        glOrtho(0.0, Texture.Width, 0.0, Texture.Height, -150.0, 150.0);

        glMatrixMode(GL_MODELVIEW);

        result = true;
    }

    return result;
}

void CGLFrameBuffer::Draw(int x, int y)
{
    DEBUG_TRACE_FUNCTION;
    if (g_GL.CanUseFrameBuffer && m_Ready)
    {
        g_GL.OldTexture = 0;
        g_GL.GL1_Draw(Texture, x, y);
    }
}

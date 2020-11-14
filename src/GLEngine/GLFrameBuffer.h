// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "GLTexture.h"
#include "../Point.h"

class CGLFrameBuffer
{
private:
    bool m_Ready = false;
#ifndef NEW_RENDERER_ENABLED
    GLint m_OldFrameBuffer = 0;
    GLuint m_FrameBuffer = 0;
#else
    static frame_buffer_t m_OldFrameBuffer;
    frame_buffer_t m_FrameBuffer = {};
    uint32_t m_Width = 0;
    uint32_t m_Height = 0;
#endif

public:
    CGLFrameBuffer();
    ~CGLFrameBuffer();

#ifndef NEW_RENDERER_ENABLED
    CGLTexture Texture{ CGLTexture() };
#endif

    bool Init(int width, int height);
    bool Init(const CSize &size) { return Init(size.Width, size.Height); }
    void Free();
    void Release();
    bool Ready() const { return m_Ready; }
    bool Ready(int width, int height);
    bool Ready(const CSize &size) { return Ready(size.Width, size.Height); }
    bool ReadyMinSize(int width, int height);
    bool ReadyMinSize(const CSize &size) { return Ready(size.Width, size.Height); }
    bool Use();
    void Draw(int x, int y);
};

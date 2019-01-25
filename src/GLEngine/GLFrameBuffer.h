// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GLTexture.h"

class CGLFrameBuffer
{
private:
    bool m_Ready = false;
    GLint m_OldFrameBuffer = 0;
    GLuint m_FrameBuffer = 0;

public:
    CGLFrameBuffer();
    ~CGLFrameBuffer();

    CGLTexture Texture{ CGLTexture() };

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

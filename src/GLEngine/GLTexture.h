// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GLHeaders.h"

struct CGLTexture
{
    uint16_t Width = 0;
    uint16_t Height = 0;
    GLuint VertexBuffer = 0;
    GLuint MirroredVertexBuffer = 0;
    GLuint Texture = 0;

    void Draw_Tooltip(int x, int y, int width, int height);
    virtual void Draw(int x, int y, bool checktrans = false);
    virtual void Draw(int x, int y, int width, int height, bool checktrans = false);
    virtual void DrawRotated(int x, int y, float angle);
    virtual void DrawTransparent(int x, int y, bool stencil = true);
    virtual void Clear();

    CGLTexture() = default;
    virtual ~CGLTexture();
};

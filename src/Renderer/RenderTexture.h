// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <external/gfx/gfx.h>
#include "RenderTypes.h"

struct CRenderTexture
{
    uint16_t Width = 0;
    uint16_t Height = 0;
    // GLuint VertexBuffer = 0;
    // GLuint MirroredVertexBuffer = 0;
    texture_handle_t Texture = RENDER_TEXTUREHANDLE_INVALID;
    // layer inside the shared art texture array once this texture has been
    // copied into it (-1 = not in the array, Texture stays the sampler2D)
    // ArtU/ArtV are the uv scale to use against the array (size / layer size)
    int ArrayLayer = -1;
    float ArtU = 1.0f;
    float ArtV = 1.0f;

    void Draw_Tooltip(int x, int y, int width, int height);
    virtual void Draw(int x, int y, bool checktrans = false);
    virtual void Draw(int x, int y, int width, int height, bool checktrans = false);
    virtual void DrawRotated(int x, int y, float angle);
    virtual void DrawTransparent(int x, int y, bool stencil = true);
    virtual void Clear();

    // world statics / flat land: lazily copy this sprite into the shared art
    // texture array so same-state draws merge; the layer is cached on the
    // texture (-1 = not placed, the individual texture stays the sampler)
    int ArtArrayPlace();

    CRenderTexture() = default;
    virtual ~CRenderTexture();
};

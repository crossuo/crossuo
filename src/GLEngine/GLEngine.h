// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../Renderer/RenderAPI.h"
#ifndef NEW_RENDERER_ENABLED

#include <deque>
#include "../Platform.h"
#include "../Point.h"

typedef std::deque<CRect> SCISSOR_LIST;
typedef uint32_t texture_handle_t;

struct CGLTexture;
class CLandObject;
class CGLEngine
{
public:
    HDC DC = 0;
    HGLRC RC = 0;
    bool Drawing = false;
    texture_handle_t OldTexture = 0;
    float SittingCharacterOffset = 8.0f;
    SDL_GLContext m_context;

private:
    SCISSOR_LIST m_ScissorList;

public:
    CGLEngine();
    virtual ~CGLEngine();

    bool Install();
    void Uninstall();
    void UpdateRect();
    void BeginDraw();
    void EndDraw();
    void ViewPortScaled(int x, int y, int width, int height);
    void ViewPort(int x, int y, int width, int height);
    void RestorePort();

    void PushScissor(int x, int y, int width, int height);
    void PushScissor(const CPoint2Di &position, int width, int height);
    void PushScissor(int x, int y, const CSize &size);
    void PushScissor(const CPoint2Di &position, const CSize &size);
    void PushScissor(const CRect &rect);

    void PopScissor();

    void ClearScissorList();

    inline void BindTexture(texture_handle_t texture);
    void DrawLine(int x, int y, int targetX, int targetY);
    void DrawPolygone(int x, int y, int width, int height);
    void DrawCircle(float x, float y, float radius, int gradientMode = 0);
    void BindTexture16(CGLTexture &texture, int width, int height, uint16_t *pixels);
    void BindTexture32(CGLTexture &texture, int width, int height, uint32_t *pixels);
    void DrawLandTexture(const CGLTexture &texture, int x, int y, CLandObject *land);
    void Draw(const CGLTexture &texture, int x, int y);
    void DrawRotated(const CGLTexture &texture, int x, int y, float angle);
    void DrawMirrored(const CGLTexture &texture, int x, int y, bool mirror);
    void DrawSitting(
        const CGLTexture &texture,
        int x,
        int y,
        bool mirror,
        float h3mod,
        float h6mod,
        float h9mod);
    void DrawShadow(const CGLTexture &texture, int x, int y, bool mirror);
    void DrawStretched(const CGLTexture &texture, int x, int y, int drawWidth, int drawHeight);
    void DrawResizepic(CGLTexture **th, int x, int y, int width, int height);
};

extern CGLEngine g_GL;

#endif // NEW_RENDERER_ENABLED
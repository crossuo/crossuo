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

    virtual void Draw(int x, int y, bool checktrans = false);
    virtual void Draw(int x, int y, int width, int height, bool checktrans = false);
    virtual void DrawRotated(int x, int y, float angle);
    virtual void DrawTransparent(int x, int y, bool stencil = true);
    virtual void Clear();

    CGLTexture() = default;
    virtual ~CGLTexture();
};

struct WEB_LINK_RECT
{
    uint16_t LinkID;
    int StartX;
    int StartY;
    int EndX;
    int EndY;
};

// FIXME
struct CGLTextTexture : public CGLTexture
{
    int LinesCount = 0;

    bool Empty() { return (Texture == 0); }
    virtual void Clear() override
    {
        CGLTexture::Clear();
        LinesCount = 0;
    }

    virtual void ClearWebLink() {}
    virtual void AddWebLink(WEB_LINK_RECT &wl) {}
    virtual uint16_t WebLinkUnderMouse(int x, int y) { return 0; }

    CGLTextTexture() = default;
    virtual ~CGLTextTexture() = default;
};

struct CGLHTMLTextTexture : public CGLTextTexture
{
    std::deque<WEB_LINK_RECT> m_WebLinkRect;

    uint16_t WebLinkUnderMouse(int x, int y);

    virtual void ClearWebLink() override { m_WebLinkRect.clear(); }
    virtual void AddWebLink(WEB_LINK_RECT &wl) override { m_WebLinkRect.push_back(wl); }

    CGLHTMLTextTexture() = default;
    virtual ~CGLHTMLTextTexture() { m_WebLinkRect.clear(); }
};

// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include <stdint.h>
#include <vector>
#include <deque>

struct CGLTexture;

struct CSprite
{
    int16_t Width = 0;
    int16_t Height = 0;
    int16_t ImageOffsetX = 0;
    int16_t ImageOffsetY = 0;
    int16_t ImageWidth = 0;
    int16_t ImageHeight = 0;
    std::vector<bool> m_HitMap;
    CGLTexture *Texture = nullptr;

    void LoadSprite16(int width, int height, uint16_t *pixels, bool skipHitMask = false);
    void LoadSprite32(int width, int height, uint32_t *pixels, bool skipHitMask = false);

    virtual void Clear();
    virtual bool Select(int x, int y, bool pixelCheck = true);
    virtual bool TestHit(int x, int y, bool pixelCheck = true);

    template <typename T>
    void Init(int width, int height, T *pixels, bool skipHitMask);

    template <typename T>
    void BuildHitMask(int w, int h, T *pixels);

    CSprite() = default;
    virtual ~CSprite() { Clear(); };
};

struct WEB_LINK_RECT
{
    uint16_t LinkID;
    int StartX;
    int StartY;
    int EndX;
    int EndY;
};

struct CTextSprite : public CSprite
{
    int LinesCount = 0;
    virtual bool Empty() { return LinesCount == 0; }

    void Init(int width, int height, uint32_t *pixels, bool skipHitMask = false);

    virtual void Clear() override
    {
        LinesCount = 0;
        CSprite::Clear();
    }

    virtual void Draw(int x, int y, bool checktrans = false);
    void Draw_Tooltip(int x, int y, int width, int height);

    virtual void ClearWebLink() {}
    virtual void AddWebLink(WEB_LINK_RECT &wl) {}
    virtual uint16_t WebLinkUnderMouse(int x, int y) { return 0; }

    CTextSprite() = default;
    virtual ~CTextSprite() { Clear(); };
};

struct CHTMLText : public CTextSprite
{
    std::deque<WEB_LINK_RECT> m_WebLinkRect;

    virtual uint16_t WebLinkUnderMouse(int x, int y) override;

    virtual void ClearWebLink() override { m_WebLinkRect.clear(); }
    virtual void AddWebLink(WEB_LINK_RECT &wl) override { m_WebLinkRect.push_back(wl); }

    CHTMLText() = default;
    virtual ~CHTMLText() { m_WebLinkRect.clear(); }
};

// GPLv3 License
// Copyright (C) 2019 Danny Angelo Carminati Grein

#pragma once

#include <vector>

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

    virtual ~CSprite() = default;
};

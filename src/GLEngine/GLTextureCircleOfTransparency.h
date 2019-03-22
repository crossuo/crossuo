// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../Sprite.h"

// FIXME: gfx
struct CGLTextureCircleOfTransparency
{
    CSprite m_Sprite;

    int Radius = 0;
    int X = 0;
    int Y = 0;

    CGLTextureCircleOfTransparency() = default;
    virtual ~CGLTextureCircleOfTransparency();

    bool Create(int radius);
    void Redraw();

    virtual void Draw(int x, int y, bool checktrans = false);
};

extern CGLTextureCircleOfTransparency g_CircleOfTransparency;

std::vector<uint32_t> CreateCircleSprite(int radius, int16_t &width, int16_t &height);
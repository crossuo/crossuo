// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include <cmath> // sqrt
#include "RenderCircleOfTransparency.h"
#include "../Managers/ConfigManager.h"
#include "RenderAPI.h"
#include "../Utility/PerfMarker.h"

CRenderCircleOfTransparency g_CircleOfTransparency;

std::vector<uint32_t> CreateCircleSprite(int radius, int16_t &width, int16_t &height)
{
    int fixRadius = radius + 1;
    int mulRadius = fixRadius * 2;
    std::vector<uint32_t> pixels;
    pixels.resize(mulRadius * mulRadius);
    width = mulRadius;
    height = mulRadius;
    for (int x = -fixRadius; x < fixRadius; x++)
    {
        intptr_t mulX = x * x;
        int posX = (((int)x + fixRadius) * mulRadius) + fixRadius;
        for (int y = -fixRadius; y < fixRadius; y++)
        {
            int r = (int)sqrt(mulX + (y * y));
            uint8_t pic = ((r <= radius) ? ((radius - r) & 0xFF) : 0);
            int pos = posX + (int)y;
            pixels[pos] = pic;
        }
    }
    return pixels;
}

CRenderCircleOfTransparency::~CRenderCircleOfTransparency()
{
    m_Sprite.Clear();
}

bool CRenderCircleOfTransparency::Create(int radius)
{
    if (radius <= 0)
    {
        return false;
    }

    if (radius > 200)
    {
        radius = 200;
    }

    if (radius == Radius)
    {
        return true;
    }

    int16_t w = 0, h = 0;
    auto pixels = CreateCircleSprite(radius, w, h);
    Radius = radius;
    m_Sprite.Clear();
    m_Sprite.LoadSprite32(w, h, pixels.data());
    return true;
}

void CRenderCircleOfTransparency::Draw(int x, int y, bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
    if (m_Sprite.Texture == nullptr)
    {
        return;
    }

    X = x - m_Sprite.Width / 2;
    Y = y - m_Sprite.Height / 2;

    SCOPED_GL_DEBUG_MARKER_LABEL("CircleOfTransparency");

    // the checktrans shader evaluates the circle analytically
    // (uCircleTrans + uFragToGame), no stencil mask is needed
    RenderAdd_SetCircleTrans(
        g_renderCmdList, SetCircleTransCmd{ float(x), float(y), float(Radius) });
}

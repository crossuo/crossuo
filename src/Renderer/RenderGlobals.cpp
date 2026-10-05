// AGPLv3 License
// Copyright (C) 2026 Danny Angelo Carminati Grein

#include "../Renderer/RenderAPI.h"

#include <float.h>

float4 g_ColorWhite = { 1.f, 1.f, 1.f, 1.f };
float4 g_ColorBlack = { 0.f, 0.f, 0.f, 1.f };
float4 g_ColorBlue = { 0.f, 0.f, 1.f, 1.f };

// Sentinel for "no color set on this command", meant to be distinguishable
// from any real color. It must be the largest finite float: bit-pattern based
// sentinels either produce NaN (0xffffffff poisons later arithmetic and makes
// every comparison false) or collide with a valid color (0x00000000 has the
// bits of +0.0f, i.e. plain black).
float4 g_ColorInvalid = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };

RenderFrameStats g_frameStats;

void Render_GetFrameStats(RenderFrameStats *stats)
{
    *stats = g_frameStats;
}

void Render_ResetFrameStats()
{
    g_frameStats = RenderFrameStats{};
}

int Render_GetCurrentDrawMode()
{
#if defined(USE_GL3) || defined(USE_GLES)
    if (g_renderCmdList != nullptr)
    {
        return g_renderCmdList->state.currentDrawMode;
    }
#endif
    return 0;
}


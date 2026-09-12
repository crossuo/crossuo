// AGPLv3 License
// Copyright (C) 2026 Danny Angelo Carminati Grein

#include "../Renderer/RenderAPI.h"
#if defined(NEW_RENDERER_ENABLED)

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

#endif // #if defined(NEW_RENDERER_ENABLED)

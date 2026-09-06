// AGPLv3 License
// Copyright (C) 2026 Danny Angelo Carminati Grein

#include "../Renderer/RenderAPI.h"
#if defined(NEW_RENDERER_ENABLED)

float4 g_ColorWhite = { 1.f, 1.f, 1.f, 1.f };
float4 g_ColorBlack = { 0.f, 0.f, 0.f, 1.f };
float4 g_ColorBlue = { 0.f, 0.f, 1.f, 1.f };

static int g_iColorInvalid = 0xffffffff;
float4 g_ColorInvalid = { *(float *)&g_iColorInvalid,
                          *(float *)&g_iColorInvalid,
                          *(float *)&g_iColorInvalid,
                          *(float *)&g_iColorInvalid };

#endif // #if defined(NEW_RENDERER_ENABLED)

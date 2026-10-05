// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "../Renderer/RenderAPI.h"
#if defined(USE_GL1)

#include "Renderer/RenderTypes.h"
extern RenderCmdList *g_renderCmdList;
extern float4 g_ColorWhite;
extern float4 g_ColorBlack;
extern float4 g_ColorBlue;
extern float4 g_ColorInvalid;

typedef uint32_t texture_handle_t;

class CGLEngine
{
public:
    bool Drawing = false;
    SDL_GLContext m_context;

public:
    CGLEngine();
    virtual ~CGLEngine();

    void Uninstall();
    void BeginDraw();
    void EndDraw();
};

extern CGLEngine g_GL;

#endif // defined(USE_GL1)

// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <external/gfx/gfx.h>
#include "RenderAPI.h"

struct CRenderShader
{
#if defined(USE_GL1)
    GLuint m_Shader = 0;
    GLuint m_VertexShader = 0;
    GLuint m_FragmentShader = 0;
    GLuint m_TexturePointer = 0;
    // Colorize
    GLuint m_ColorTablePointer = 0;
    GLuint m_DrawModePointer = 0;
#else
    ShaderPipeline m_ShaderPipeline;
    uint32_t m_TexturePointer = 0;
    // Colorize
    uint32_t m_ColorTablePointer = 0;
    uint32_t m_DrawModePointer = 0;
#endif
    int m_Type = 1;
    bool Init(const char *vertexShaderData, const char *fragmentShaderData, int type);
    void Enable() const;
    void Disable();
    void Pause();
    void Resume();
    ~CRenderShader();
};

extern CRenderShader g_DeathShader;
extern CRenderShader g_ColorizerShader;
extern CRenderShader g_FontColorizerShader;
extern CRenderShader g_LightColorizerShader;

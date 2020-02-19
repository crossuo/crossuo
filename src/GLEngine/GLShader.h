// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GLHeaders.h"
#include "../Renderer/RenderAPI.h"

struct CGLShader
{
#ifndef NEW_RENDERER_ENABLED
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
    void Enable();
    void Disable();
    void Pause();
    void Resume();
    ~CGLShader();
};

extern CGLShader g_DeathShader;
extern CGLShader g_ColorizerShader;
extern CGLShader g_FontColorizerShader;
extern CGLShader g_LightColorizerShader;

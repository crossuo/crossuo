// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Renderer/RenderAPI.h"

void UnuseShader();

class CGLShader
{
protected:
    ShaderPipeline m_ShaderPipeline{};

    uint32_t m_TexturePointer{ 0 };

public:
    CGLShader();
    virtual ~CGLShader();

    virtual bool Init(const char *vertexShaderData, const char *fragmentShaderData);

    virtual bool Use();

    virtual void Pause();

    virtual void Resume();
};

class CDeathShader : public CGLShader
{
public:
    CDeathShader();

    virtual bool Init(const char *vertexShaderData, const char *fragmentShaderData);
};

class CColorizerShader : public CGLShader
{
protected:
    uint32_t m_ColorTablePointer{ 0 };
    uint32_t m_DrawModePointer{ 0 };

public:
    CColorizerShader();

    virtual bool Init(const char *vertexShaderData, const char *fragmentShaderData);

    virtual bool Use();
};

extern CDeathShader g_DeathShader;
extern CColorizerShader g_ColorizerShader;
extern CColorizerShader g_FontColorizerShader;
extern CColorizerShader g_LightColorizerShader;

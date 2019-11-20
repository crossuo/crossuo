// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GLHeaders.h"
#include "../Renderer/RenderAPI.h"

void UnuseShader();

class CGLShader
{
protected:
#ifndef NEW_RENDERER_ENABLED
    GLuint m_Shader{ 0 };

    GLuint m_VertexShader{ 0 };

    GLuint m_FragmentShader{ 0 };

    //Указатель на текстуру для передачи ее шейдеру (можно не использовать, текстура передается автоматически при glBingTexture)
    GLuint m_TexturePointer{ 0 };
#else
    ShaderPipeline m_ShaderPipeline{};

    uint32_t m_TexturePointer{ 0 };
#endif

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
#ifndef NEW_RENDERER_ENABLED
    //Указатель на палитру цветов для передачи ее шейдеру
    GLuint m_ColorTablePointer{ 0 };

    //Указатель на метод вывода шейдером
    GLuint m_DrawModePointer{ 0 };
#else
    uint32_t m_ColorTablePointer{ 0 };
    uint32_t m_DrawModePointer{ 0 };
#endif

public:
    CColorizerShader();

    virtual bool Init(const char *vertexShaderData, const char *fragmentShaderData);

    virtual bool Use();
};

extern CDeathShader g_DeathShader;
extern CColorizerShader g_ColorizerShader;
extern CColorizerShader g_FontColorizerShader;
extern CColorizerShader g_LightColorizerShader;

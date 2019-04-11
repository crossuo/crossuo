// MIT License
// Copyright (C) August 2016 Hotride

#include "Renderer/RenderAPI.h"
extern RenderCmdList *g_renderCmdList;

CDeathShader g_DeathShader;
CColorizerShader g_ColorizerShader;
CColorizerShader g_FontColorizerShader;
CColorizerShader g_LightColorizerShader;

void UnuseShader()
{
    DEBUG_TRACE_FUNCTION;
    RenderAdd_DisableShaderPipeline(g_renderCmdList);
    ShaderColorTable = 0;
    g_ShaderDrawMode = 0;
}

CGLShader::CGLShader()
{
    DEBUG_TRACE_FUNCTION;
}

bool CGLShader::Init(const char *vertexShaderData, const char *fragmentShaderData)
{
    if (vertexShaderData == nullptr && fragmentShaderData == nullptr)
    {
        return false;
    }

    if (!Render_CreateShaderPipeline(vertexShaderData, fragmentShaderData, &m_ShaderPipeline))
    {
        return false;
    }

    Info(Renderer, "Shaders created successfully");
    return true;
}

CGLShader::~CGLShader()
{
    DEBUG_TRACE_FUNCTION;
    Render_DestroyShaderPipeline(&m_ShaderPipeline);

    m_TexturePointer = 0;
}

bool CGLShader::Use()
{
    DEBUG_TRACE_FUNCTION;
    // TODO useless?
    UnuseShader();

    bool result = false;

    if (m_ShaderPipeline.program != RENDER_SHADERPROGRAM_INVALID)
    {
        RenderAdd_SetShaderPipeline(g_renderCmdList, &m_ShaderPipeline);
        result = true;
    }

    return result;
}

void CGLShader::Pause()
{
    DEBUG_TRACE_FUNCTION;
    RenderAdd_DisableShaderPipeline(g_renderCmdList);
}

void CGLShader::Resume()
{
    DEBUG_TRACE_FUNCTION;
    if (m_ShaderPipeline.program != RENDER_SHADERPROGRAM_INVALID)
    {
        RenderAdd_SetShaderPipeline(g_renderCmdList, &m_ShaderPipeline);
    }
}

CDeathShader::CDeathShader()
    : CGLShader()
{
    DEBUG_TRACE_FUNCTION;
}

bool CDeathShader::Init(const char *vertexShaderData, const char *fragmentShaderData)
{
    if (CGLShader::Init(vertexShaderData, fragmentShaderData))
    {
        m_TexturePointer = Render_GetUniformId(&m_ShaderPipeline, "usedTexture");
        return true;
    }

    Error(Renderer, "Failed to create DeathShader");
    assert(false);
    return false;
}

CColorizerShader::CColorizerShader()
    : CGLShader()
{
    DEBUG_TRACE_FUNCTION;
}

bool CColorizerShader::Init(const char *vertexShaderData, const char *fragmentShaderData)
{
    if (CGLShader::Init(vertexShaderData, fragmentShaderData))
    {
        m_TexturePointer = Render_GetUniformId(&m_ShaderPipeline, "usedTexture");
        m_ColorTablePointer = Render_GetUniformId(&m_ShaderPipeline, "colors");
        m_DrawModePointer = Render_GetUniformId(&m_ShaderPipeline, "drawMode");
        return true;
    }

    Error(Renderer, "Failed to create ColorizerShader");
    assert(false);
    return false;
}

bool CColorizerShader::Use()
{
    DEBUG_TRACE_FUNCTION;
    bool result = CGLShader::Use();

    if (result)
    {
        ShaderColorTable = m_ColorTablePointer;
        g_ShaderDrawMode = m_DrawModePointer;
        auto uniformValue = SDM_NO_COLOR;
        RenderAdd_SetShaderUniform(
            g_renderCmdList,
            &RenderAdd_ShaderUniformCmd(g_ShaderDrawMode, &uniformValue, ShaderUniformType::Int1));
    }

    return result;
}

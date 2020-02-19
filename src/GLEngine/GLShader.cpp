// MIT License
// Copyright (C) August 2016 Hotride

#include "GLShader.h"
#include "GLHeaders.h"
#include <assert.h>
#include <common/logging/logging.h>
#include "../Globals.h" // g_ShaderDrawMode / SDM_NO_COLOR, g_ShaderColorTableInUse

CGLShader g_DeathShader;
CGLShader g_ColorizerShader;
CGLShader g_FontColorizerShader;
CGLShader g_LightColorizerShader;

bool CGLShader::Init(const char *vertexShaderData, const char *fragmentShaderData, int type)
{
    if (vertexShaderData == nullptr && fragmentShaderData == nullptr)
    {
        return false;
    }
    m_Type = type;
#ifndef NEW_RENDERER_ENABLED
    auto validate_shader_compile = [](GLuint shader) {
        auto val = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &val);
        if (val != GL_TRUE)
        {
            GLint logLength;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
            auto compileLog = (GLchar *)malloc(logLength * sizeof(GLcharARB));
            assert(compileLog);
            glGetShaderInfoLog(shader, logLength, nullptr, compileLog);
            Error(Renderer, "shader compilation error, compile log:\n%s", compileLog);
            free(compileLog);
            return false;
        }
        return true;
    };

    auto validate_program = [](GLuint program, GLenum validation_type) {
        assert(validation_type == GL_LINK_STATUS || validation_type == GL_VALIDATE_STATUS);
        auto val = GL_FALSE;
        glGetProgramiv(program, validation_type, &val);
        if (val != GL_TRUE)
        {
            GLint logLength;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
            auto programLog = (GLchar *)malloc(logLength * sizeof(GLchar));
            assert(programLog);
            glGetProgramInfoLog(program, logLength, nullptr, programLog);
            Error(Renderer, "program log:\n%s", programLog);
            free(programLog);
            return false;
        }
        return true;
    };

    auto link_program = [&](GLuint program) {
        glLinkProgram(program);
        if (!validate_program(program, GL_LINK_STATUS))
        {
            Error(Renderer, "shader link failed");
            return false;
        }

        glValidateProgram(program);
        if (!validate_program(program, GL_VALIDATE_STATUS))
        {
            Error(Renderer, "shader validate failed");
            return false;
        }
        return true;
    };

    auto create_shader =
        [&](GLuint program, GLenum shaderType, const GLchar *source, GLuint *shader) {
            *shader = glCreateShader(shaderType);
            assert(*shader != 0);
            glShaderSource(*shader, 1, &source, nullptr);
            glCompileShader(*shader);
            if (!validate_shader_compile(*shader))
            {
                return false;
            }
            glAttachShader(program, *shader);
            return true;
        };

    m_Shader = glCreateProgram();
    assert(m_Shader != 0);

    if (!create_shader(m_Shader, GL_VERTEX_SHADER, vertexShaderData, &m_VertexShader))
    {
        return false;
    }

    if (!create_shader(m_Shader, GL_FRAGMENT_SHADER, fragmentShaderData, &m_FragmentShader))
    {
        return false;
    }

    if (!link_program(m_Shader))
    {
        glDeleteProgram(m_Shader);
        m_Shader = 0;
        return false;
    }

    if (type == 0 /* death */)
    {
        m_TexturePointer = glGetUniformLocationARB(m_Shader, "usedTexture");
        return (m_Shader != 0);
    }

    if (type == 1 /* colorize */)
    {
        m_TexturePointer = glGetUniformLocationARB(m_Shader, "usedTexture");
        m_ColorTablePointer = glGetUniformLocationARB(m_Shader, "colors");
        m_DrawModePointer = glGetUniformLocationARB(m_Shader, "drawMode");
        return (m_Shader != 0);
    }
    Error(Renderer, "failed to create shader, unknown type %d", type);
    return false;
#else
    if (!Render_CreateShaderPipeline(vertexShaderData, fragmentShaderData, &m_ShaderPipeline))
    {
        Error(Renderer, "failed to create shader");
        return false;
    }

    if (type == 0 /* death */)
    {
        m_TexturePointer = Render_GetUniformId(&m_ShaderPipeline, "usedTexture");
        return true;
    }

    if (type == 1 /* colorize */)
    {
        m_TexturePointer = Render_GetUniformId(&m_ShaderPipeline, "usedTexture");
        m_ColorTablePointer = Render_GetUniformId(&m_ShaderPipeline, "colors");
        m_DrawModePointer = Render_GetUniformId(&m_ShaderPipeline, "drawMode");
        return true;
    }
    Error(Renderer, "failed to create shader, unknown type %d", type);
    assert(false);
    return false;
#endif
}

CGLShader::~CGLShader()
{
#ifndef NEW_RENDERER_ENABLED
    if (m_Shader != 0)
    {
        glDeleteObjectARB(m_Shader);
        m_Shader = 0;
    }

    if (m_VertexShader != 0)
    {
        glDeleteObjectARB(m_VertexShader);
        m_VertexShader = 0;
    }

    if (m_FragmentShader != 0)
    {
        glDeleteObjectARB(m_FragmentShader);
        m_FragmentShader = 0;
    }
#else
    Render_DestroyShaderPipeline(&m_ShaderPipeline);
#endif
    m_TexturePointer = 0;
}

void CGLShader::Enable()
{
    bool result = false;
#ifndef NEW_RENDERER_ENABLED
    // TODO useless?
    // UnuseShader();
    if (m_Shader != 0)
    {
        glUseProgram(m_Shader);
        result = true;
    }
#else
    if (m_ShaderPipeline.program != RENDER_SHADERPROGRAM_INVALID)
    {
        RenderAdd_SetShaderPipeline(g_renderCmdList, &m_ShaderPipeline);
        result = true;
    }
#endif

    if (result && m_Type == 1)
    {
        //assert(m_ColorTablePointer);
        g_ShaderColorTableInUse = m_ColorTablePointer;
        g_ShaderDrawMode = m_DrawModePointer;
#ifndef NEW_RENDERER_ENABLED
        glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
#else
        ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
        cmd.value.asInt1 = SDM_NO_COLOR;
        RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
    }
}

void CGLShader::Disable()
{
#ifndef NEW_RENDERER_ENABLED
    glUseProgramObjectARB(0);
#else
    RenderAdd_DisableShaderPipeline(g_renderCmdList);
#endif
    g_ShaderColorTableInUse = 0;
    g_ShaderDrawMode = 0;
}

void CGLShader::Pause()
{
#ifndef NEW_RENDERER_ENABLED
    glUseProgramObjectARB(0);
#else
    RenderAdd_DisableShaderPipeline(g_renderCmdList);
#endif
}

void CGLShader::Resume()
{
#ifndef NEW_RENDERER_ENABLED
    glUseProgramObjectARB(m_Shader);
#else
    // if (m_ShaderPipeline.program != RENDER_SHADERPROGRAM_INVALID)
    {
        RenderAdd_SetShaderPipeline(g_renderCmdList, &m_ShaderPipeline);
    }
#endif
}

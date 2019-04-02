// MIT License
// Copyright (C) August 2016 Hotride

CDeathShader g_DeathShader;
CColorizerShader g_ColorizerShader;
CColorizerShader g_FontColorizerShader;
CColorizerShader g_LightColorizerShader;

void UnuseShader()
{
    DEBUG_TRACE_FUNCTION;
    glUseProgramObjectARB(0);
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
    Info(Renderer, "shaders linked successfully");

    return true;
}

CGLShader::~CGLShader()
{
    DEBUG_TRACE_FUNCTION;
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

    m_TexturePointer = 0;
}

bool CGLShader::Use()
{
    DEBUG_TRACE_FUNCTION;
    UnuseShader();

    bool result = false;

    if (m_Shader != 0)
    {
        glUseProgram(m_Shader);
        result = true;
    }

    return result;
}

void CGLShader::Pause()
{
    DEBUG_TRACE_FUNCTION;
    glUseProgramObjectARB(0);
}

void CGLShader::Resume()
{
    DEBUG_TRACE_FUNCTION;
    glUseProgramObjectARB(m_Shader);
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
        m_TexturePointer = glGetUniformLocationARB(m_Shader, "usedTexture");
    }
    else
    {
        Info(Renderer, "Failed to create DeathShader");
    }

    return (m_Shader != 0);
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
        m_TexturePointer = glGetUniformLocationARB(m_Shader, "usedTexture");
        m_ColorTablePointer = glGetUniformLocationARB(m_Shader, "colors");
        m_DrawModePointer = glGetUniformLocationARB(m_Shader, "drawMode");
    }
    else
    {
        Info(Renderer, "Failed to create ColorizerShader");
    }

    return (m_Shader != 0);
}

bool CColorizerShader::Use()
{
    DEBUG_TRACE_FUNCTION;
    bool result = CGLShader::Use();

    if (result)
    {
        ShaderColorTable = m_ColorTablePointer;
        g_ShaderDrawMode = m_DrawModePointer;
        glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
    }

    return result;
}

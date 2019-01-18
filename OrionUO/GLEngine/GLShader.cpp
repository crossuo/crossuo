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
    GLint val = GL_FALSE;

    if (vertexShaderData != nullptr && fragmentShaderData != nullptr)
    {
        m_Shader = glCreateProgramObjectARB();

        m_VertexShader = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
        glShaderSourceARB(m_VertexShader, 1, (const GLcharARB **)&vertexShaderData, nullptr);
        glCompileShaderARB(m_VertexShader);

        glGetShaderiv(m_VertexShader, GL_COMPILE_STATUS, &val);
        if (val != GL_TRUE)
        {
            LOG("CGLShader::Init vertex shader compilation error:\n");
            auto error = glGetError();
            if (error != 0)
            {
                LOG("CGLShader::Init vertex shader error Code: %i\n", error);
            }
            return false;
        }
        LOG("vertex shader compiled successfully\n");

        glAttachObjectARB(m_Shader, m_VertexShader);

        m_FragmentShader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
        glShaderSourceARB(m_FragmentShader, 1, (const GLcharARB **)&fragmentShaderData, nullptr);
        glCompileShaderARB(m_FragmentShader);

        glGetShaderiv(m_FragmentShader, GL_COMPILE_STATUS, &val);
        if (val != GL_TRUE)
        {
            LOG("CGLShader::Init fragment shader compilation error:\n");
            auto error = glGetError();
            if (error != 0)
            {
                LOG("CGLShader::Init fragment shader error Code: %i\n", error);
            }
            return false;
        }
        LOG("fragment shader compiled successfully\n");

        glAttachObjectARB(m_Shader, m_FragmentShader);

        glLinkProgramARB(m_Shader);
        glValidateProgramARB(m_Shader);
    }
    else
    {
        return false;
    }

    glGetShaderiv(m_Shader, GL_COMPILE_STATUS, &val);
    if (val != GL_TRUE)
    {
        LOG("CGLShader::Init shader program compilation error:\n");
        auto error = glGetError();
        if (error != 0)
        {
            LOG("CGLShader::Init shader program error Code: %i\n", error);
        }
        return false;
    }
    LOG("shader program compiled successfully\n");

    GLint isLinked = 0;
    glGetProgramiv(m_Shader, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetProgramiv(m_Shader, GL_INFO_LOG_LENGTH, &maxLength);

        // The maxLength includes the nullptr character
        vector<GLchar> infoLog(maxLength);
        glGetProgramInfoLog(m_Shader, maxLength, &maxLength, &infoLog[0]);

        // The program is useless now. So delete it.
        glDeleteProgram(m_Shader);

        LOG("shader program failed to link\n");
        LOG("%s\n", infoLog.data());
        wstring str(infoLog.begin(), infoLog.end());
        LOG("%ws\n", str.c_str());
        return false;
    }

    return val == GL_TRUE;
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
        LOG("Failed to create DeathShader\n");
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
        LOG("Failed to create ColorizerShader\n");
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

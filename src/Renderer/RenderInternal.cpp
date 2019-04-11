#include "Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "Renderer/RenderInternal.h"

float4 g_ColorWhite = { { 1.f, 1.f, 1.f, 1.f } };

float float4::operator[](size_t i)
{
    assert(i < countof(rgba));
    return rgba[i];
}

uint32_t Render_ShaderUniformTypeToSize(ShaderUniformType type)
{
    static uint32_t s_uniformTypeToSize[] = {
        uint32_t(sizeof(uint32_t)), // ShaderUniformType::Int1
        0xffffffff,                 // ShaderUniformType::Float1V
    };

    static_assert(countof(s_uniformTypeToSize) == ShaderUniformType::ShaderUniform_Count);

    return s_uniformTypeToSize[type];
}

ShaderUniformCmd::ShaderUniformCmd(uint32_t id, void *value, ShaderUniformType type)
    : header{ RenderCommandType::Cmd_ShaderUniform }
    , id(id)
    , type(type)
{
    memcpy(this->value, value, Render_ShaderUniformTypeToSize(type));
}

bool Render_CreateShaderPipeline(
    const char *vertexShaderSource, const char *fragmentShaderSource, ShaderPipeline *pipeline)
{
    assert(vertexShaderSource);
    assert(fragmentShaderSource);
    assert(pipeline);

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

    ShaderPipeline &pip = *pipeline;
    pip = ShaderPipeline{};
    pip.program = glCreateProgram();
    assert(pip.program != 0);

    if (!create_shader(
            pip.program,
            GL_VERTEX_SHADER,
            vertexShaderSource,
            &pip.shaders[ShaderStage::VertexShader]))
    {
        return false;
    }

    if (!create_shader(
            pip.program,
            GL_FRAGMENT_SHADER,
            fragmentShaderSource,
            &pip.shaders[ShaderStage::FragmentShader]))
    {
        return false;
    }

    if (!link_program(pip.program))
    {
        glDeleteProgram(pip.program);
        pip.program = 0;
        return false;
    }

    return true;
}

bool Render_DestroyShaderPipeline(ShaderPipeline *pipeline)
{
    assert(pipeline);
    if (pipeline->program != RENDER_SHADERPROGRAM_INVALID)
    {
        for (auto handle : pipeline->shaders)
        {
            if (handle != RENDER_SHADERHANDLE_INVALID)
            {
                glDeleteShader(handle);
            }
        }

        glDeleteProgram(pipeline->program);
    }

    *pipeline = ShaderPipeline{};
    return true;
}

uint32_t Render_GetUniformId(ShaderPipeline *pipeline, const char *uniform)
{
    assert(pipeline);
    assert(uniform);

    if (!pipeline->program)
    {
        return RENDER_SHADERUNIFORMID_INVALID;
    }

    if (pipeline->uniformCount + 1 > countof(pipeline->uniforms))
    {
        Error(
            Renderer,
            "Not enough space for shader uniforms. Increase RENDER_MAX_SHADERPIPELINE_UNIFORM (%d)",
            RENDER_MAX_SHADERPIPELINE_UNIFORM);
        assert(false);
        return RENDER_SHADERUNIFORMID_INVALID;
    }

    auto uniformId = pipeline->uniformCount;
    pipeline->uniformCount++;

    pipeline->uniforms[uniformId].location = glGetUniformLocation(pipeline->program, uniform);

    return uniformId;
}

bool Render_AppendCmd(RenderCmdList *cmdList, void *cmd, uint32_t cmdSize)
{
    assert(cmdList);
    assert(cmd);
    assert(cmdSize);

    if (cmdList->remainingSize >= cmdSize)
    {
        memcpy(cmdList->data + cmdList->size - cmdList->remainingSize, cmd, cmdSize);
        cmdList->remainingSize -= cmdSize;
        return true;
    }

    Error(Renderer, __FUNCTION__ " render cmd list capacity reached. skipping render cmd %p", cmd);
    return false;
}

RenderCmdList
Render_CmdList(void *buffer, uint32_t bufferSize, RenderState state, bool immediateMode)
{
    assert(buffer);
    assert(bufferSize);

    return RenderCmdList(buffer, bufferSize, state, immediateMode);
}

RenderState Render_DefaultState()
{
    return RenderState();
}

void Render_ResetCmdList(RenderCmdList *cmdList, RenderState state)
{
    assert(cmdList);

    cmdList->remainingSize = cmdList->size;
    cmdList->state = state;
}
#include "Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "Renderer/RenderInternal.h"
#include "Utility/PerfMarker.h"

float4 g_ColorWhite = { 1.f, 1.f, 1.f, 1.f };
float4 g_ColorBlack = { 0.f, 0.f, 0.f, 1.f };
float4 g_ColorBlue = { 0.f, 0.f, 1.f, 1.f };
static int g_iColorInvalid = 0xffffffff;
float4 g_ColorInvalid = { *(float *)&g_iColorInvalid,
                          *(float *)&g_iColorInvalid,
                          *(float *)&g_iColorInvalid,
                          *(float *)&g_iColorInvalid };

struct
{
    SDL_GLContext context = nullptr;
    SDL_Window *window = nullptr;
} g_render;

float float4::operator[](size_t i) const
{
    assert(i < countof(rgba));
    return rgba[i];
}

bool float4::operator==(float4 &other)
{
    return memcmp(rgba, other.rgba, sizeof(rgba)) == 0;
}

bool float4::operator!=(float4 &other)
{
    return !(*this == other);
}

float float3::operator[](size_t i) const
{
    assert(i < countof(rgb));
    return rgb[i];
}

bool float3::operator==(float3 &other)
{
    return memcmp(rgb, other.rgb, sizeof(rgb)) == 0;
}

bool float3::operator!=(float3 &other)
{
    return !(*this == other);
}

bool Render_Init(SDL_Window *window)
{
#ifdef OGL_DEBUGCONTEXT_ENABLED
    auto debugContext = true;
#else
    auto debugContext = false;
#endif
    if (debugContext)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    }

    auto context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    int glewInitResult = glewInit();
    Info(
        Renderer,
        "glewInit() = %i fb=%i v(%s) (shader: %i)",
        glewInitResult,
        GL_ARB_framebuffer_object,
        glGetString(GL_VERSION),
        GL_ARB_shader_objects);
    if (glewInitResult != 0)
    {
        SDL_GL_DeleteContext(context);
        return false;
    }

    // debug messages callback needs ogl >= 4.30
    // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDebugMessageControl.xhtml
    if (debugContext && GLEW_KHR_debug)
    {
        SetupOGLDebugMessage();
    }

    Info(Renderer, "Graphics Successfully Initialized");
    Info(Renderer, "OpenGL Info:");
    Info(Renderer, "    Version: %s", glGetString(GL_VERSION));
    Info(Renderer, "     Vendor: %s", glGetString(GL_VENDOR));
    Info(Renderer, "   Renderer: %s", glGetString(GL_RENDERER));
    Info(Renderer, "    Shading: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

    auto canUseFrameBuffer =
        (GL_ARB_framebuffer_object && glBindFramebuffer && glDeleteFramebuffers &&
         glFramebufferTexture2D && glGenFramebuffers);

    if (!canUseFrameBuffer)
    {
        SDL_GL_DeleteContext(context);
        Error(Client, "Your graphics card does not support Frame Buffers");
        return false;
    }
    // glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black Background
    glShadeModel(GL_SMOOTH); // Enables Smooth Color Shading
    glClearDepth(1.0);       // Depth Buffer Setup
    glDisable(GL_DITHER);

    //glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);   //Realy Nice perspective calculations
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

    glEnable(GL_TEXTURE_2D);

    SDL_GL_SetSwapInterval(0); // 1 vsync

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    glClearStencil(0);
    // glStencilMask(1);

    glEnable(GL_LIGHT0);

    GLfloat lightPosition[] = { -1.0f, -1.0f, 0.5f, 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, &lightPosition[0]);

    GLfloat lightAmbient[] = { 2.0f, 2.0f, 2.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, &lightAmbient[0]);

    GLfloat lav = 0.8f;
    GLfloat lightAmbientValues[] = { lav, lav, lav, lav };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, &lightAmbientValues[0]);

    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

    g_render.context = context;
    g_render.window = window;
    return true;
}

void Render_Shutdown()
{
    if (g_render.context != nullptr)
    {
        SDL_GL_DeleteContext(g_render.context);
    }
}

bool HACKRender_SetViewParams(const SetViewParamsCmd &cmd)
{
    ScopedPerfMarker(__FUNCTION__);

    // game viewport isn't scaled, if the OS window is smaller than scene_y + scene_height, bottom will
    // be negative by this difference
    int needed_height = cmd.scene_y + cmd.scene_height;
    int bottom = cmd.window_height - needed_height;

    glViewport(cmd.scene_x, bottom, cmd.scene_width, cmd.scene_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(
        cmd.scene_x,
        cmd.scene_x + cmd.scene_width,
        cmd.scene_y + cmd.scene_height,
        cmd.scene_y,
        cmd.camera_nearZ,
        cmd.camera_farZ);

    glMatrixMode(GL_MODELVIEW);
    return true;
}

bool HACKRender_GetFrameBuffer(RenderCmdList *cmdList, frame_buffer_t *currFb)
{
    assert(currFb);
    *currFb = cmdList->state.framebuffer;
    return true;
}

void Render_SwapBuffers()
{
    SDL_GL_SwapWindow(g_render.window);
}

uint32_t Render_ShaderUniformTypeToSize(ShaderUniformType type)
{
    static uint32_t s_uniformTypeToSize[] = {
        uint32_t(sizeof(uint32_t)), // ShaderUniformType::ShaderUniformType_Int1
        0xffffffff,                 // ShaderUniformType::ShaderUniformType_Float1V
    };

    assert(type < ShaderUniformType_VariableFirst);

    static_assert(
        countof(s_uniformTypeToSize) == ShaderUniformType::ShaderUniformType_Count,
        "missing uniform types");

    return s_uniformTypeToSize[type];
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
            &pip.shaders[ShaderStage::ShaderStage_VertexShader]))
    {
        return false;
    }

    if (!create_shader(
            pip.program,
            GL_FRAGMENT_SHADER,
            fragmentShaderSource,
            &pip.shaders[ShaderStage::ShaderStage_FragmentShader]))
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

texture_handle_t Render_CreateTexture2D(
    uint32_t width,
    uint32_t height,
    TextureGPUFormat gpuFormat,
    void *pixels,
    TextureFormat pixelsFormat)
{
    static GLenum s_gpuFormatToOGLFormat[] = {
        GL_RGBA4,   // RGBA4
        GL_RGB5_A1, // RGB5_A1
    };

    static GLenum s_pixelFormatToOGLFormat[] = {
        GL_UNSIGNED_INT_8_8_8_8,       // TextureFormat_Unsigned_RGBA8
        GL_UNSIGNED_INT_8_8_8_8_REV,   // TextureFormat_Unsigned_ABGR8
        GL_UNSIGNED_SHORT_1_5_5_5_REV, // TextureFormat_Unsigned_A1_BGR5
    };

    texture_handle_t tex = RENDER_TEXTUREHANDLE_INVALID;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        s_gpuFormatToOGLFormat[gpuFormat],
        width,
        height,
        0,
        GL_BGRA,
        s_pixelFormatToOGLFormat[pixelsFormat],
        pixels);

    return tex;
}

frame_buffer_t Render_CreateFrameBuffer(uint32_t width, uint32_t height)
{
    texture_handle_t texture;
    framebuffer_handle_t handle;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, nullptr);

    GLint currFb = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currFb);

    glGenFramebuffers(1, &handle);
    glBindFramebuffer(GL_FRAMEBUFFER, handle);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    frame_buffer_t fb = {};
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
    {
        fb.texture = texture;
        fb.handle = handle;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, currFb);
    return fb;
}

bool Render_DestroyFrameBuffer(frame_buffer_t fb)
{
    auto validTex = fb.texture != RENDER_TEXTUREHANDLE_INVALID;
    auto validFb = fb.handle != RENDER_FRAMEBUFFER_INVALID;
    assert(validFb);
    assert(validTex);
    if (validTex)
    {
        glDeleteTextures(1, &fb.texture);
    }

    if (validFb)
    {
        glDeleteFramebuffers(1, &fb.handle);
    }

    return validTex && validFb;
}

bool Render_DestroyTexture(texture_handle_t texture)
{
    assert(texture != RENDER_TEXTUREHANDLE_INVALID);
    if (texture != RENDER_TEXTUREHANDLE_INVALID)
    {
        glDeleteTextures(1, &texture);
        return true;
    }

    return false;
}

bool Render_AppendCmd(RenderCmdList *cmdList, const void *cmd, uint32_t cmdSize)
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

    Error(
        Renderer, "%s render cmd list capacity reached. skipping render cmd %p", __FUNCTION__, cmd);
    return false;
}

bool Render_AppendCmdType(
    RenderCmdList *cmdList, RenderCommandType type, const void *cmd, uint32_t cmdSize)
{
    static_assert(sizeof(type) == sizeof(uint8_t), "command type id is assumed to be byte sized");
    if (cmdList->remainingSize >= sizeof(type) + cmdSize)
    {
        auto ptr = cmdList->data + cmdList->size - cmdList->remainingSize;
        *ptr = (uint8_t)type;
        ptr++;

        memcpy(ptr, cmd, cmdSize);
        cmdList->remainingSize -= sizeof(type) + cmdSize;
        return true;
    }

    Error(
        Renderer, "%s render cmd list capacity reached. skipping render cmd %p", __FUNCTION__, cmd);
    return false;
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
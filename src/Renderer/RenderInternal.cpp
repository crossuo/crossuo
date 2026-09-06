// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Everton Fernando Patitucci da Silva

#include "../Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "../Renderer/RenderInternal.h"
#include "../Utility/PerfMarker.h"
#include <common/logging/logging.h>
#include <assert.h>
#include <string.h> // memcmp, memcpy
#include <stdlib.h> // malloc
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#define countof(xarray) (sizeof(xarray) / sizeof(xarray[0]))

#if defined(USE_GLES) || defined(USE_GL3)
// clang-format off
static const uint32_t _missingTexture[] = {
    0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff,
    0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000,
    0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff,
    0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000,
    0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff,
    0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000,
    0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff,
    0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000, 0xff00ffff, 0x00000000,
};
// clang-format on
#include "../ShaderData.h"
extern uint32_t _vao;
extern uint32_t _vbo;
extern uint32_t _vio;
extern uint32_t _defaultTex;
extern int _inPos;
extern int _inColor;
extern int _inUV;
extern int _uProjectionView;
extern int _uModel;
extern int _uTex;
extern int _pProg;
extern int _pProgLand;
extern int _inNormalLand;
extern int _uDrawModeLand;
extern int _uColorsLand;
uint32_t _vao = 0;
uint32_t _vibuffers[2] = { 0, 0 };
uint32_t _defaultTex = 0;
int _inPos = 0;
int _inColor = 0;
int _inUV = 0;
int _uProjectionView = 0;
int _uModel = 0;
int _uTex = 0;
int _pProg = 0;
int _pProgLand = 0;
int _inNormalLand = 0;
int _uDrawModeLand = 0;
int _uColorsLand = 0;
#endif

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
    // set when the GL context is gone; global/static destructors may still try
    // to release GL resources afterwards (destruction order at exit is not
    // guaranteed), and issuing GL calls on a dead context corrupts the driver
    // state and crashes the process
    bool shutdown = false;
} g_render;

float float4::operator[](size_t i) const
{
    assert(i < countof(rgba));
    return rgba[i];
}

bool float4::operator==(const float4 &other) const
{
    return memcmp(rgba, other.rgba, sizeof(rgba)) == 0;
}

bool float4::operator!=(const float4 &other) const
{
    return !(*this == other);
}

float float3::operator[](size_t i) const
{
    assert(i < countof(rgb));
    return rgb[i];
}

bool float3::operator==(const float3 &other) const
{
    return memcmp(rgb, other.rgb, sizeof(rgb)) == 0;
}

bool float3::operator!=(const float3 &other) const
{
    return !(*this == other);
}

bool Render_Init(SDL_Window *window)
{
    win_gfx_context_attrbutes(true);
    auto context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);

    // mark the renderer as down on ANY process exit path (atexit handlers run
    // in reverse order, so this executes before the global/static destructors),
    // guarding the Render_Destroy* calls against issuing GL commands without a
    // live context
    atexit([] { g_render.shutdown = true; });

#if defined(USE_GLEW)
    int glewInitResult = glewInit();
    if (glewInitResult != 0)
    {
        SDL_GL_DeleteContext(context);
        Error(Renderer, "glewInit: %s", glewGetErrorString(glewInitResult));
        return false;
    }
    Info(
        Renderer,
        "glew(%s), fb=%i v(%s) (shader: %i)",
        glewGetString(GLEW_VERSION),
        GL_ARB_framebuffer_object,
        glGetString(GL_VERSION),
        GL_ARB_shader_objects);
#elif defined(USE_GL3W)
    if (gl3wInit() != 0)
    {
        Error(Renderer, "could not initialize opengl\n");
        return false;
    }
#endif

    Info(Renderer, "Graphics Successfully Initialized");
    Info(Renderer, "Renderer:");
    Info(Renderer, "    Version: %s", glGetString(GL_VERSION));
    Info(Renderer, "     Vendor: %s", glGetString(GL_VENDOR));
    Info(Renderer, "   Renderer: %s", glGetString(GL_RENDERER));
    Info(Renderer, "    Shading: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));

#if defined(USE_GL)
#ifdef OGL_DEBUGCONTEXT_ENABLED
    // debug messages callback needs ogl >= 4.30
    // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDebugMessageControl.xhtml
#if defined(USE_GLEW)
    if (GLEW_KHR_debug)
#else
    if (GL_KHR_debug)
#endif
    {
        SetupOGLDebugMessage();
    }
#endif

    const auto canUseFrameBuffer =
        (GL_ARB_framebuffer_object && glBindFramebuffer && glDeleteFramebuffers &&
         glFramebufferTexture2D && glGenFramebuffers);
#else
    SetupOGLDebugMessage();
    const auto canUseFrameBuffer = true;
#endif

    if (!canUseFrameBuffer)
    {
        SDL_GL_DeleteContext(context);
        Error(Client, "Your graphics card does not support Frame Buffers");
        return false;
    }

    SDL_GL_SetSwapInterval(0); // 1 vsync
    GL_CHECK(glClearStencil(0));
    // glStencilMask(1);
    // glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Black Background
#if defined(USE_GL2)
    GL_CHECK(glEnable(GL_TEXTURE_2D));
    GL_CHECK(glShadeModel(GL_SMOOTH)); // Enables Smooth Color Shading
    GL_CHECK(glClearDepth(1.0));       // Depth Buffer Setup
    GL_CHECK(glDisable(GL_DITHER));
    //glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);   //Realy Nice perspective calculations
    GL_CHECK(glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST));
    GL_CHECK(glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL));
    GL_CHECK(glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE));
    GL_CHECK(glEnable(GL_LIGHT0));
    const GLfloat lightPosition[] = { -1.0f, -1.0f, 0.5f, 0.0f };
    GL_CHECK(glLightfv(GL_LIGHT0, GL_POSITION, &lightPosition[0]));
    const GLfloat lightAmbient[] = { 2.0f, 2.0f, 2.0f, 1.0f };
    GL_CHECK(glLightfv(GL_LIGHT0, GL_AMBIENT, &lightAmbient[0]));
    const GLfloat lav = 0.8f;
    const GLfloat lightAmbientValues[] = { lav, lav, lav, lav };
    GL_CHECK(glLightModelfv(GL_LIGHT_MODEL_AMBIENT, &lightAmbientValues[0]));
    GL_CHECK(glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE));
#else
    GL_CHECK(glClearDepthf(1.0)); // Depth Buffer Setup
    // TODO: gles - init shaders
    // https://www.khronos.org/webgl/wiki/WebGL_and_OpenGL_Differences
    char msg[512];
    const auto vs = glCreateShader(GL_VERTEX_SHADER);
    GL_CHECK(glShaderSource(vs, 1, &g_vShader, nullptr));
    GL_CHECK(glCompileShader(vs));
    GLint status = GL_TRUE;
    GL_CHECK(glGetShaderiv(vs, GL_COMPILE_STATUS, &status));
    if (status == GL_FALSE)
    {
        glGetShaderInfoLog(vs, sizeof(msg), nullptr, msg);
        Error(Renderer, "vs compilation: %s", msg);
    }

    const auto ps = glCreateShader(GL_FRAGMENT_SHADER);
    GL_CHECK(glShaderSource(ps, 1, &g_pShader, nullptr));
    GL_CHECK(glCompileShader(ps));
    status = GL_TRUE;
    GL_CHECK(glGetShaderiv(ps, GL_COMPILE_STATUS, &status));
    if (status == GL_FALSE)
    {
        glGetShaderInfoLog(ps, sizeof(msg), nullptr, msg);
        Error(Renderer, "ps compilation: %s", msg);
    }

    _pProg = glCreateProgram();
    GL_CHECK(glAttachShader(_pProg, vs));
    GL_CHECK(glAttachShader(_pProg, ps));
    GL_CHECK(glLinkProgram(_pProg));
    status = GL_TRUE;
    GL_CHECK(glGetProgramiv(_pProg, GL_LINK_STATUS, &status));
    if (status == GL_FALSE)
    {
        glGetProgramInfoLog(ps, sizeof(msg), nullptr, msg);
        Error(Renderer, "program link: %s", msg);
    }

    _inPos = glGetAttribLocation(_pProg, "inPos");
    GL_CHECK_ATTRIB(_inPos);
    _inUV = glGetAttribLocation(_pProg, "inUV");
    GL_CHECK_ATTRIB(_inUV);
    _inColor = glGetAttribLocation(_pProg, "inColor");
    GL_CHECK_ATTRIB(_inColor);
    _uProjectionView = glGetUniformLocation(_pProg, "uProjectionView");
    GL_CHECK_ATTRIB(_uProjectionView);
    _uModel = glGetUniformLocation(_pProg, "uModel");
    GL_CHECK_ATTRIB(_uModel);
    _uTex = glGetUniformLocation(_pProg, "uTex");
    GL_CHECK_ATTRIB(_uTex);
    GL_CHECK(glUseProgram(_pProg));

    // clang-format off
    const GenericVertex data[] = {
        { { -1.0f, -1.0f }, { 0.0f, 0.0f }, 0xff00ffff },
        { { -1.0f,  1.0f }, { 0.0f, 1.0f }, 0xff00ffff },
        { {  1.0f,  1.0f }, { 1.0f, 1.0f }, 0xff00ffff },
        { {  1.0f, -1.0f }, { 1.0f, 1.0f }, 0xff00ffff },
    };
    const unsigned int idx[] = { 0, 1, 2, 3 };
#if !defined(USE_GLES2)
    GL_CHECK(glGenVertexArrays(1, &_vao));
    GL_CHECK(glBindVertexArray(_vao));
#endif // #if !defined(USE_GLES2)
    GL_CHECK(glGenBuffers(2, _vibuffers));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, _vibuffers[0]));
    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(GenericVertex), data, GL_STATIC_DRAW));
    GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _vibuffers[1]));
    GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, 4 * sizeof(unsigned int), idx, GL_STATIC_DRAW));

    GL_CHECK(glEnableVertexAttribArray(_inPos));
    GL_CHECK(glEnableVertexAttribArray(_inUV));
    GL_CHECK(glEnableVertexAttribArray(_inColor));
    GL_CHECK(glVertexAttribPointer(_inPos, 2, GL_FLOAT, GL_FALSE, sizeof(GenericVertex), (GLvoid*)OFFSETOF(GenericVertex, pos)));
    GL_CHECK(glVertexAttribPointer(_inUV, 2, GL_FLOAT, GL_FALSE, sizeof(GenericVertex), (GLvoid*)OFFSETOF(GenericVertex, uv)));
    GL_CHECK(glVertexAttribPointer(_inColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GenericVertex), (GLvoid*)OFFSETOF(GenericVertex, col)));

    GL_CHECK(glGenTextures(1, &_defaultTex));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, _defaultTex));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, _missingTexture));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    GL_CHECK(glUniform1i(_uTex, 0)); // texture unit 0
    GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
    GL_CHECK(glUseProgram(0));
    // clang-format on

    // Initialize land tile shader with normals
    const auto vsLand = glCreateShader(GL_VERTEX_SHADER);
    GL_CHECK(glShaderSource(vsLand, 1, &g_vShaderLand, nullptr));
    GL_CHECK(glCompileShader(vsLand));
    status = GL_TRUE;
    GL_CHECK(glGetShaderiv(vsLand, GL_COMPILE_STATUS, &status));
    if (status == GL_FALSE)
    {
        glGetShaderInfoLog(vsLand, sizeof(msg), nullptr, msg);
        Error(Renderer, "land vs compilation: %s", msg);
    }

    const auto psLand = glCreateShader(GL_FRAGMENT_SHADER);
    GL_CHECK(glShaderSource(psLand, 1, &g_pShaderLand, nullptr));
    GL_CHECK(glCompileShader(psLand));
    status = GL_TRUE;
    GL_CHECK(glGetShaderiv(psLand, GL_COMPILE_STATUS, &status));
    if (status == GL_FALSE)
    {
        glGetShaderInfoLog(psLand, sizeof(msg), nullptr, msg);
        Error(Renderer, "land ps compilation: %s", msg);
    }

    _pProgLand = glCreateProgram();
    GL_CHECK(glAttachShader(_pProgLand, vsLand));
    GL_CHECK(glAttachShader(_pProgLand, psLand));
    GL_CHECK(glLinkProgram(_pProgLand));
    status = GL_TRUE;
    GL_CHECK(glGetProgramiv(_pProgLand, GL_LINK_STATUS, &status));
    if (status == GL_FALSE)
    {
        glGetProgramInfoLog(psLand, sizeof(msg), nullptr, msg);
        Error(Renderer, "land program link: %s", msg);
    }

    _inPos = glGetAttribLocation(_pProgLand, "inPos");
    _inUV = glGetAttribLocation(_pProgLand, "inUV");
    _inColor = glGetAttribLocation(_pProgLand, "inColor");
    _inNormalLand = glGetAttribLocation(_pProgLand, "inNormal");
    _uProjectionView = glGetUniformLocation(_pProgLand, "uProjectionView");
    _uModel = glGetUniformLocation(_pProgLand, "uModel");
    _uTex = glGetUniformLocation(_pProgLand, "uTex");
    _uDrawModeLand = glGetUniformLocation(_pProgLand, "drawMode");
    _uColorsLand = glGetUniformLocation(_pProgLand, "colors");
    GL_CHECK(glUseProgram(_pProgLand));
    GL_CHECK(glUniform1i(_uTex, 0)); // texture unit 0
    GL_CHECK(glUseProgram(0));
#endif
    g_render.context = context;
    g_render.window = window;
    return true;
}

void Render_Shutdown()
{
    g_render.shutdown = true;
    if (g_render.context != nullptr)
    {
        SDL_GL_DeleteContext(g_render.context);
        g_render.context = nullptr;
    }
}

bool HACKRender_SetViewParams(const SetViewParamsCmd &cmd)
{
    ScopedPerfMarker(__FUNCTION__);

    // game viewport isn't scaled, if the OS window is smaller than scene_y + scene_height, bottom will
    // be negative by this difference
    int needed_height = cmd.scene_y + cmd.scene_height;
    int bottom = cmd.window_height - needed_height;

    GL_CHECK(glViewport(cmd.scene_x, bottom, cmd.scene_width, cmd.scene_height));
#if defined(USE_GL2)
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
#else
    // TODO: gles - ortho viewparms hack
    const auto projection = glm::ortho(
        float(cmd.scene_x),
        float(cmd.scene_x + cmd.scene_width),
        float(cmd.scene_y + cmd.scene_height),
        float(cmd.scene_y),
        float(cmd.camera_nearZ),
        float(cmd.camera_farZ));
    GL_CHECK(glUseProgram(_pProg));
    GL_CHECK(glUniformMatrix4fv(_uProjectionView, 1, false, glm::value_ptr(projection)));
    GL_CHECK(glUseProgram(0));
#endif
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
        GL_CHECK(glGetShaderiv(shader, GL_COMPILE_STATUS, &val));
        if (val != GL_TRUE)
        {
            GLint logLength;
            GL_CHECK(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength));
            auto compileLog = (GLchar *)malloc(logLength * sizeof(GLchar));
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
        GL_CHECK(glGetProgramiv(program, validation_type, &val));
        if (val != GL_TRUE)
        {
            GLint logLength;
            GL_CHECK(glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength));
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
        GL_CHECK(glLinkProgram(program));
        if (!validate_program(program, GL_LINK_STATUS))
        {
            Error(Renderer, "shader link failed");
            return false;
        }

        GL_CHECK(glValidateProgram(program));
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

            GL_CHECK(glShaderSource(*shader, 1, &source, nullptr));
            GL_CHECK(glCompileShader(*shader));

            if (!validate_shader_compile(*shader))
            {
                return false;
            }

            GL_CHECK(glAttachShader(program, *shader));
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
        GL_CHECK(glDeleteProgram(pip.program));
        pip.program = 0;
        return false;
    }

    return true;
}

bool Render_DestroyShaderPipeline(ShaderPipeline *pipeline)
{
    assert(pipeline);
    if (g_render.shutdown || g_render.context == nullptr)
    {
        return false; // GL context is gone, GL resource release is irrelevant
    }

    if (pipeline->program != RENDER_SHADERPROGRAM_INVALID)
    {
        for (auto handle : pipeline->shaders)
        {
            if (handle != RENDER_SHADERHANDLE_INVALID)
            {
                GL_CHECK(glDeleteShader(handle));
            }
        }

        GL_CHECK(glDeleteProgram(pipeline->program));
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

#if defined(USE_GL)
    static GLenum s_pixelFormatToOGLFormat[] = {
        GL_UNSIGNED_INT_8_8_8_8,       // TextureFormat_Unsigned_RGBA8
        GL_UNSIGNED_SHORT_1_5_5_5_REV, // TextureFormat_Unsigned_A1_BGR5
    };
    const auto imgFormat = GL_BGRA;
#else
    static GLenum s_pixelFormatToOGLFormat[] = {
        GL_UNSIGNED_BYTE,          // TextureFormat_Unsigned_RGBA8
        GL_UNSIGNED_SHORT_5_5_5_1, // TextureFormat_Unsigned_A1_BGR5
    };
    const auto imgFormat = GL_RGBA; //GL_BGRA_EXT;
#endif

    texture_handle_t tex = RENDER_TEXTUREHANDLE_INVALID;

    GL_CHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
    GL_CHECK(glGenTextures(1, &tex));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, tex));
#if defined(USE_GL2)
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#else
    // TODO: gles - not needed
#endif
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexImage2D(
        GL_TEXTURE_2D,
        0,
        s_gpuFormatToOGLFormat[gpuFormat],
        width,
        height,
        0,
        imgFormat,
        s_pixelFormatToOGLFormat[pixelsFormat],
        pixels));

    return tex;
}

frame_buffer_t Render_CreateFrameBuffer(uint32_t width, uint32_t height)
{
    texture_handle_t texture;
    framebuffer_handle_t handle;

    GL_CHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
    GL_CHECK(glGenTextures(1, &texture));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, texture));
#if defined(USE_GL)
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, nullptr);
#else
    GL_CHECK(glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, nullptr));
#endif
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLint currFb = 0;
    GL_CHECK(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currFb));

    GL_CHECK(glGenFramebuffers(1, &handle));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, handle));

    GL_CHECK(
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0));

    frame_buffer_t fb = {};
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
    {
        fb.texture = texture;
        fb.handle = handle;
    }

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, currFb));
    return fb;
}

bool Render_DestroyFrameBuffer(frame_buffer_t fb)
{
    if (g_render.shutdown || g_render.context == nullptr)
    {
        return false; // GL context is gone, GL resource release is irrelevant
    }

    auto validTex = fb.texture != RENDER_TEXTUREHANDLE_INVALID;
    auto validFb = fb.handle != RENDER_FRAMEBUFFER_INVALID;
    assert(validFb);
    assert(validTex);
    if (validTex)
    {
        GL_CHECK(glDeleteTextures(1, &fb.texture));
    }

    if (validFb)
    {
        GL_CHECK(glDeleteFramebuffers(1, &fb.handle));
    }

    return validTex && validFb;
}

bool Render_DestroyTexture(texture_handle_t texture)
{
    assert(texture != RENDER_TEXTUREHANDLE_INVALID);
    if (g_render.shutdown || g_render.context == nullptr)
    {
        return false; // GL context is gone, GL resource release is irrelevant
    }

    if (texture != RENDER_TEXTUREHANDLE_INVALID)
    {
        GL_CHECK(glDeleteTextures(1, &texture));
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

// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Everton Fernando Patitucci da Silva

#include "../Renderer/RenderAPI.h"
#if defined(NEW_RENDERER_ENABLED) && (defined(USE_GL3) || defined(USE_GLES))
#define RENDERER_INTERNAL
#include "../Renderer/RenderInternal.h"
#include "../Renderer/RenderBatch.h"
#include "Debug/RenderDebug.h"
#include "../Utility/PerfMarker.h"
#include <common/logging/logging.h>
#include <xuocore/enumlist.h> // SDM_NO_COLOR
#include <assert.h>
#include <string.h> // memcmp, memcpy
#include <stdlib.h> // malloc
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include "ShaderData.h"
#define countof(xarray) (sizeof(xarray) / sizeof(xarray[0]))

// clang-format off
// SDL_PIXELFORMAT_ABGR8888
static const uint32_t _missingTexture[] = {
    0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff,
    0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000,
    0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff,
    0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000,
    0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff,
    0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000,
    0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff,
    0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000, 0xffff00ff, 0x00000000,
};
static const uint32_t _whiteTexture[] = {
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
};
// clang-format on
extern uint32_t _vao;
extern uint32_t _vbo;
extern uint32_t _vio;
extern uint32_t _defaultTex;
extern uint32_t _whiteTex;
extern int _inPos;
extern int _inColor;
extern int _inUV;
extern int _inNormal;
extern int _uProjectionView;
extern int _uModel;
uint32_t _vao = 0;
uint32_t _vibuffers[2] = { 0, 0 };
uint32_t _defaultTex = 0;
uint32_t _whiteTex = 0;
int _inPos = 0;
int _inLayer = 0;
int _inColor = 0;
int _inUV = 0;
int _inNormal = 0;
int _uTexArray = 0;
int _uProjectionView = 0;
int _uModel = 0;
int _uTex = 0;
int _pProg = 0;


int _uAlphaTestEnabled = 0;
int _uAlphaRef = 0;
int _uDrawMode = 0;
int _uColors = 0;
int _uCircleTrans = 0;
int _uViewport = 0;

bool g_rendererDebugForceStateReset = false;

// Persistent vertex buffers for optimized rendering (avoid per-draw allocation)
uint32_t g_drawVAO = 0;
uint32_t g_drawVBO = 0;
size_t g_vboSize = 0;
size_t g_vboOffset = 0;
const size_t MAX_VERTICES = 65536; // Support up to 64k vertices



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



bool Render_Init(SDL_Window *window)
{
    // mark the renderer as down on ANY process exit path (atexit handlers run
    // in reverse order, so this executes before the global/static destructors),
    // guarding the Render_Destroy* calls against issuing GL commands without a
    // live context
    atexit([] { g_render.shutdown = true; });
    win_gfx_context_attrbutes(true);
    auto context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);

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

#if defined(USE_GL3) && defined(OGL_DEBUGCONTEXT_ENABLED)
    GLint maxStackDepth;
    glGetIntegerv(GL_MAX_DEBUG_GROUP_STACK_DEPTH, &maxStackDepth);
    Info(Renderer, "    Markers Depth: %d", maxStackDepth);
#endif // #if defined(USE_GL3) && defined(OGL_DEBUGCONTEXT_ENABLED)

#if defined(USE_GL)
#ifdef OGL_DEBUGCONTEXT_ENABLED
    // debug messages callback needs ogl >= 4.30
    // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glDebugMessageControl.xhtml
#if defined(USE_GLEW)
    if (GLEW_KHR_debug)
#else
    if (GL_KHR_debug)
#endif // defined(USE_GLEW)
    {
        SetupOGLDebugMessage();
    }
#endif // OGL_DEBUGCONTEXT_ENABLED

    const auto canUseFrameBuffer =
        (GL_ARB_framebuffer_object && glBindFramebuffer && glDeleteFramebuffers &&
         glFramebufferTexture2D && glGenFramebuffers);
#else // defined(USE_GL)
    SetupOGLDebugMessage();
    const auto canUseFrameBuffer = true;
#endif // defined(USE_GL)

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
    _inNormal = glGetAttribLocation(_pProg, "inNormal");
    GL_CHECK_ATTRIB(_inNormal);
    _uProjectionView = glGetUniformLocation(_pProg, "uProjectionView");
    GL_CHECK_ATTRIB(_uProjectionView);
    _uModel = glGetUniformLocation(_pProg, "uModel");
    GL_CHECK_ATTRIB(_uModel);
    _uTex = glGetUniformLocation(_pProg, "uTex");
    GL_CHECK_ATTRIB(_uTex);
    _uAlphaTestEnabled = glGetUniformLocation(_pProg, "uAlphaTestEnabled");
    GL_CHECK_ATTRIB(_uAlphaTestEnabled);
    _uAlphaRef = glGetUniformLocation(_pProg, "uAlphaRef");
    GL_CHECK_ATTRIB(_uAlphaRef);
    _uDrawMode = glGetUniformLocation(_pProg, "drawMode");
    GL_CHECK_ATTRIB(_uDrawMode);
    _uColors = glGetUniformLocation(_pProg, "colors");
    GL_CHECK_ATTRIB(_uColors);
    _uCircleTrans = glGetUniformLocation(_pProg, "uCircleTrans");
    GL_CHECK_ATTRIB(_uCircleTrans);
    _uViewport = glGetUniformLocation(_pProg, "uViewport");
    GL_CHECK_ATTRIB(_uViewport);
    _uTexArray = glGetUniformLocation(_pProg, "uTexArray");
    GL_CHECK_ATTRIB(_uTexArray);
    _inLayer = glGetAttribLocation(_pProg, "inLayer");
    GL_CHECK_ATTRIB(_inLayer);
    GL_CHECK(glUseProgram(_pProg));

    // clang-format off
    const GenericVertex data[] = {
        { { -1.0f, -1.0f }, { 0.0f, 0.0f }, 0xffff00ff, { 0.0f, 0.0f, 1.0f }, -1 },
        { { -1.0f,  1.0f }, { 0.0f, 1.0f }, 0xffff00ff, { 0.0f, 0.0f, 1.0f }, -1 },
        { {  1.0f,  1.0f }, { 1.0f, 1.0f }, 0xffff00ff, { 0.0f, 0.0f, 1.0f }, -1 },
        { {  1.0f, -1.0f }, { 1.0f, 1.0f }, 0xffff00ff, { 0.0f, 0.0f, 1.0f }, -1 },
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
    GL_CHECK(glEnableVertexAttribArray(_inNormal));
    GL_CHECK(glVertexAttribPointer(_inPos, 2, GL_FLOAT, GL_FALSE, sizeof(GenericVertex), (GLvoid*)OFFSETOF(GenericVertex, pos)));
    GL_CHECK(glVertexAttribPointer(_inUV, 2, GL_FLOAT, GL_FALSE, sizeof(GenericVertex), (GLvoid*)OFFSETOF(GenericVertex, uv)));
    GL_CHECK(glVertexAttribPointer(_inColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GenericVertex), (GLvoid*)OFFSETOF(GenericVertex, col)));
    GL_CHECK(glVertexAttribPointer(_inNormal, 3, GL_FLOAT, GL_FALSE, sizeof(GenericVertex), (GLvoid*)OFFSETOF(GenericVertex, normal)));

    GL_CHECK(glGenTextures(1, &_defaultTex));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, _defaultTex));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, _missingTexture));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    GL_CHECK(glGenTextures(1, &_whiteTex));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, _whiteTex));
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, _whiteTexture));
    // no mipmaps are allocated, the default GL_NEAREST_MIPMAP_LINEAR filter
    // would make this texture incomplete and sample as black (GL3 core)
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    // Set texture unit to 0 once during initialization (never needs to change)
    GL_CHECK(glUniform1i(_uTex, 0)); // texture unit 0
    // texture array lives on texture unit 1 (unit 0 is the sampler2D path);
    GL_CHECK(glUniform1i(_uTexArray, 1));
    GL_CHECK(glUniform1i(_uDrawMode, SDM_NO_COLOR)); // default draw mode
    static const float initialColors[96] = { 0.0f };
    GL_CHECK(glUniform1fv(_uColors, 96, initialColors)); // Initial color palette
    GL_CHECK(glUniform1i(_uAlphaTestEnabled, 0)); // Alpha test disabled initially
    GL_CHECK(glUniform1f(_uAlphaRef, 0.0f)); // Initial alpha reference value

    // Initialize model matrix to identity
    glm::mat4 identity(1.0f);
    GL_CHECK(glUniformMatrix4fv(_uModel, 1, false, glm::value_ptr(identity)));
    GL_CHECK(glDrawArrays(GL_TRIANGLE_FAN, 0, 4));
    // clang-format on

    // Initialize persistent vertex buffers for optimized rendering
    Render_InitVertexBuffers();
    g_render.context = context;
    g_render.window = window;
    return true;
}

void Render_Shutdown()
{
    g_render.shutdown = true;
    Render_CleanupVertexBuffers();
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
    // TODO: gles - ortho viewparms hack
    const auto projection = glm::ortho(
        float(cmd.scene_x),
        float(cmd.scene_x + cmd.scene_width),
        float(cmd.scene_y + cmd.scene_height),
        float(cmd.scene_y),
        float(cmd.camera_nearZ),
        float(cmd.camera_farZ));
    // Set projection for basic shader
    GL_CHECK(glUseProgram(_pProg));
    GL_CHECK(glUniformMatrix4fv(_uProjectionView, 1, false, glm::value_ptr(projection)));
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
    // submit any pending batched quads: the last draws of a frame (e.g. the
    // mouse cursor on the login screens) would otherwise be deferred past
    // the swap and never reach the screen
    if (g_renderCmdList != nullptr && g_renderCmdList->immediateMode)
    {
        RenderBatch_Flush(&g_renderCmdList->state);
    }

    // start the next frame on a fresh buffer region: orphaning the storage
    // here lets the driver rename it while the GPU may still be reading the
    // previous frame's data, avoiding per-draw implicit syncs on overwrite
    if (g_vboOffset != 0)
    {
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, g_drawVBO));
        GL_CHECK(glBufferData(GL_ARRAY_BUFFER, g_vboSize, nullptr, GL_DYNAMIC_DRAW));
        g_vboOffset = 0;
    }
    SDL_GL_SwapWindow(g_render.window);
}

size_t Render_UploadVertices(const void *data, size_t size)
{
    if (g_vboOffset + size > g_vboSize)
    {
        // frame data exceeded the ring capacity, orphan and wrap
        GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, g_drawVBO));
        GL_CHECK(glBufferData(GL_ARRAY_BUFFER, g_vboSize, nullptr, GL_DYNAMIC_DRAW));
        g_vboOffset = 0;
    }

    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, g_drawVBO));
    GL_CHECK(glBufferSubData(GL_ARRAY_BUFFER, g_vboOffset, size, data));

    const size_t offset = g_vboOffset;
    g_vboOffset += size;
    g_frameStats.vertexUploads++;
    g_frameStats.bytesUploaded += size;
    return offset;
}

void Render_BindVertexRange(size_t byteOffset)
{
    GL_CHECK(glVertexAttribPointer(
        _inPos, 2, GL_FLOAT, GL_FALSE, sizeof(GenericVertex), (GLvoid *)(byteOffset + OFFSETOF(GenericVertex, pos))));
    GL_CHECK(glVertexAttribPointer(
        _inUV, 2, GL_FLOAT, GL_FALSE, sizeof(GenericVertex), (GLvoid *)(byteOffset + OFFSETOF(GenericVertex, uv))));
    GL_CHECK(glVertexAttribPointer(
        _inColor,
        4,
        GL_UNSIGNED_BYTE,
        GL_TRUE,
        sizeof(GenericVertex),
        (GLvoid *)(byteOffset + OFFSETOF(GenericVertex, col))));
    GL_CHECK(glVertexAttribPointer(
        _inNormal,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(GenericVertex),
        (GLvoid *)(byteOffset + OFFSETOF(GenericVertex, normal))));
    GL_CHECK(glVertexAttribPointer(
        _inLayer,
        1,
        GL_FLOAT,
        GL_FALSE,
        sizeof(GenericVertex),
        (GLvoid *)(byteOffset + OFFSETOF(GenericVertex, layer))));
}

bool Render_InitVertexBuffers()
{
    ScopedPerfMarker(__FUNCTION__);

    // VBO size based on GenericVertex format (now includes normal)
    g_vboSize = MAX_VERTICES * sizeof(GenericVertex);

#if !defined(USE_GLES2)
    GL_CHECK(glGenVertexArrays(1, &g_drawVAO));
    GL_CHECK(glBindVertexArray(g_drawVAO));
#endif

    GL_CHECK(glGenBuffers(1, &g_drawVBO));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, g_drawVBO));
    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, g_vboSize, nullptr, GL_DYNAMIC_DRAW));

    // Set up vertex attributes for unified GenericVertex format
    GL_CHECK(glEnableVertexAttribArray(_inPos));
    GL_CHECK(glVertexAttribPointer(_inPos, 2, GL_FLOAT, GL_FALSE, sizeof(GenericVertex), (GLvoid*)OFFSETOF(GenericVertex, pos)));
    GL_CHECK(glEnableVertexAttribArray(_inUV));
    GL_CHECK(glVertexAttribPointer(_inUV, 2, GL_FLOAT, GL_FALSE, sizeof(GenericVertex), (GLvoid*)OFFSETOF(GenericVertex, uv)));
    GL_CHECK(glEnableVertexAttribArray(_inColor));
    GL_CHECK(glVertexAttribPointer(_inColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GenericVertex), (GLvoid*)OFFSETOF(GenericVertex, col)));
    GL_CHECK(glEnableVertexAttribArray(_inNormal));
    GL_CHECK(glVertexAttribPointer(_inNormal, 3, GL_FLOAT, GL_FALSE, sizeof(GenericVertex), (GLvoid*)OFFSETOF(GenericVertex, normal)));
    GL_CHECK(glEnableVertexAttribArray(_inLayer));
    GL_CHECK(glVertexAttribPointer(_inLayer, 1, GL_FLOAT, GL_FALSE, sizeof(GenericVertex), (GLvoid*)OFFSETOF(GenericVertex, layer)));

#if !defined(USE_GLES2)
    GL_CHECK(glBindVertexArray(0));
#endif

    Info(Renderer, "Persistent vertex buffers initialized: %zu bytes (%zu vertices)", g_vboSize, MAX_VERTICES);
    return true;
}

void Render_CleanupVertexBuffers()
{
    ScopedPerfMarker(__FUNCTION__);

    if (g_drawVBO != 0)
    {
        GL_CHECK(glDeleteBuffers(1, &g_drawVBO));
        g_drawVBO = 0;
    }

#if !defined(USE_GLES2)
    if (g_drawVAO != 0)
    {
        GL_CHECK(glDeleteVertexArrays(1, &g_drawVAO));
        g_drawVAO = 0;
    }
#endif // #if !defined(USE_GLES2)

    g_vboSize = 0;
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
    if (g_render.shutdown || g_render.context == nullptr)
    {
        return false; // GL context is gone, GL resource release is irrelevant
    }
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
        // note: sampler-unit assignment (uTex=0, uTexArray=1) happens after
        // this point; validating before that would report a unit conflict
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

    // the shared fragment source has a sampler2D (unit 0) and a
    // sampler2DArray (unit 1); assign the units right after the link and
    // BEFORE the validate, which checks for unit conflicts
    GL_CHECK(glUseProgram(pip.program));
    GLint texLoc = glGetUniformLocation(pip.program, "uTex");
    GLint arrayLoc = glGetUniformLocation(pip.program, "uTexArray");
    GLint viewportLoc = glGetUniformLocation(pip.program, "uViewport");
    GLint circleLoc = glGetUniformLocation(pip.program, "uCircleTrans");
    if (texLoc >= 0)
    {
        GL_CHECK(glUniform1i(texLoc, 0));
    }
    if (arrayLoc >= 0)
    {
        GL_CHECK(glUniform1i(arrayLoc, 1));
    }
    if (viewportLoc >= 0)
    {
        GL_CHECK(glUniform2f(viewportLoc, 0.0f, 0.0f));
    }
    if (circleLoc >= 0)
    {
        GL_CHECK(glUniform3f(circleLoc, 0.0f, 0.0f, -1.0f)); // disabled
    }
    GL_CHECK(glValidateProgram(pip.program));
    if (!validate_program(pip.program, GL_VALIDATE_STATUS))
    {
        GL_CHECK(glDeleteProgram(pip.program));
        pip.program = 0;
        return false;
    }

    // restore the main pipeline as the active program
    GL_CHECK(glUseProgram(_pProg));

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
#else // #if defined(USE_GL)
    static GLenum s_pixelFormatToOGLFormat[] = {
        GL_UNSIGNED_BYTE,          // TextureFormat_Unsigned_RGBA8
        GL_UNSIGNED_SHORT_5_5_5_1, // TextureFormat_Unsigned_A1_BGR5
    };
    const auto imgFormat = GL_RGBA; //GL_BGRA_EXT;
#endif // #if defined(USE_GL)

    texture_handle_t tex = RENDER_TEXTUREHANDLE_INVALID;

    GL_CHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    //GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
    GL_CHECK(glGenTextures(1, &tex));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, tex));
    //GL_CHECK(glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE));

    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    //GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    //GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
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
    if (g_render.shutdown || g_render.context == nullptr)
    {
        return frame_buffer_t{}; // default = invalid handles, GL context is gone
    }
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
#if defined(USE_GL3) || defined(USE_GLES)
    // give the quad batcher a chance to flush before a state change is applied
    if (cmdList->immediateMode)
    {
        RenderBatch_OnCommand(&cmdList->state, type, cmd);
    }
#endif
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


// ---------------------------------------------------------------------------
// Land texture array (Phase B of the world batching work)
//
// Land texmaps are uniform 44x44 RGB5_A1 sprites with NEAREST filtering and
// no mipmaps, which makes them ideal for a GL_TEXTURE_2D_ARRAY: one layer per
// land tile, copied GPU->GPU from the per-tile textures on first use, so all
// land tiles can be drawn merged into a handful of calls.
// ---------------------------------------------------------------------------

static const int ART_ARRAY_LAYER_SIZE = 256; // sprites up to 256x256 fit
static const int ART_ARRAY_INITIAL_LAYERS = 1024;
static const int ART_ARRAY_MAX_LAYERS = 4096;

static uint32_t s_artArrayTexture = 0;
static int s_artArrayCap = 0;
static int s_artArrayUsed = 0;
static bool s_artArrayDisabled = false; // set when the GL limits are hit

texture_handle_t Render_ArtArrayTexture()
{
    return s_artArrayTexture;
}

// copies srcTexture (w x h) into a fresh layer of the shared art array and
// returns the uv scale to use for it (uv = size / ART_ARRAY_LAYER_SIZE);
// returns layer < 0 when the sprite cannot be placed (too big / limits hit)
RenderArrayPlacement Render_ArtArrayPlace(texture_handle_t srcTexture, uint32_t width, uint32_t height)
{
    RenderArrayPlacement place = {};
    place.layer = -1;
    place.texture = srcTexture;
    place.u = 1.0f;
    place.v = 1.0f;

    if (s_artArrayDisabled || g_render.shutdown || width == 0 || height == 0)
    {
        return place;
    }

    // the source name must still be a live texture object: the streaming layer
    // hot-swaps sprites (DestroySprite -> CGLTexture::Clear -> delete) while
    // the game runs, and a recycled/zero name here makes glCopyImageSubData
    // read an incomplete or foreign source. A non-texture name is
    // GL_INVALID_OPERATION, so leave the sprite on its own texture path
    if (srcTexture == RENDER_TEXTUREHANDLE_INVALID || !glIsTexture(srcTexture))
    {
        return place;
    }

    if (width > ART_ARRAY_LAYER_SIZE || height > ART_ARRAY_LAYER_SIZE)
    {
        return place; // oversized sprite: keep the individual texture
    }

    if (s_artArrayTexture == 0)
    {
        GLint maxLayers = 1024;
        GL_CHECK(glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxLayers));
        s_artArrayCap =
            (ART_ARRAY_INITIAL_LAYERS < maxLayers) ? ART_ARRAY_INITIAL_LAYERS : maxLayers;
        if (s_artArrayCap < 1)
        {
            s_artArrayDisabled = true;
            return place;
        }

        // build the array on unit 1 and leave it bound there (unit 0 is the
        // sampler2D path); creating it on unit 0 would leave the array name
        // bound to GL_TEXTURE_2D, and glBindTexture(GL_TEXTURE_2D, name) with
        // an array name is GL_INVALID_OPERATION (non-gen name)
        GL_CHECK(glGenTextures(1, &s_artArrayTexture));
        GL_CHECK(glActiveTexture(GL_TEXTURE1));
        GL_CHECK(glBindTexture(GL_TEXTURE_2D_ARRAY, s_artArrayTexture));
        GL_CHECK(glTexImage3D(
            GL_TEXTURE_2D_ARRAY, 0, GL_RGB5_A1, ART_ARRAY_LAYER_SIZE, ART_ARRAY_LAYER_SIZE,
            s_artArrayCap, 0, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, nullptr));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CHECK(glActiveTexture(GL_TEXTURE0));
    }

    if (s_artArrayUsed >= s_artArrayCap)
    {
        // grow the array (up to the GL limit) by recreating and copying
        if (s_artArrayCap >= ART_ARRAY_MAX_LAYERS)
        {
            s_artArrayDisabled = true;
            Warning(Renderer, "art texture array is full, falling back to per-sprite textures");
            return place;
        }

        const int newCap = s_artArrayCap * 2;
        uint32_t newArray = 0;
        // same rule as creation: build (and keep) the array on unit 1 only
        GL_CHECK(glGenTextures(1, &newArray));
        GL_CHECK(glActiveTexture(GL_TEXTURE1));
        GL_CHECK(glBindTexture(GL_TEXTURE_2D_ARRAY, newArray));
        GL_CHECK(glTexImage3D(
            GL_TEXTURE_2D_ARRAY, 0, GL_RGB5_A1, ART_ARRAY_LAYER_SIZE, ART_ARRAY_LAYER_SIZE,
            newCap, 0, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, nullptr));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CHECK(glCopyImageSubData(
            s_artArrayTexture, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0,
            newArray, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, ART_ARRAY_LAYER_SIZE,
            ART_ARRAY_LAYER_SIZE, s_artArrayUsed));

        GL_CHECK(glDeleteTextures(1, &s_artArrayTexture));
        s_artArrayTexture = newArray;
        s_artArrayCap = newCap;

        GL_CHECK(glActiveTexture(GL_TEXTURE0));
    }

    const int layer = s_artArrayUsed++;
    GL_CHECK(glCopyImageSubData(
        srcTexture, GL_TEXTURE_2D, 0, 0, 0, 0,
        s_artArrayTexture, GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, width, height, 1));
    place.layer = layer;
    place.texture = s_artArrayTexture;
    place.u = float(width) / float(ART_ARRAY_LAYER_SIZE);
    place.v = float(height) / float(ART_ARRAY_LAYER_SIZE);
    return place;
}


void Render_ResetCmdList(RenderCmdList *cmdList, RenderState state)
{
    assert(cmdList);

    cmdList->remainingSize = cmdList->size;
    cmdList->state = state;
}

#endif // #if defined(NEW_RENDERER_ENABLED) && (defined(USE_GL3) || defined(USE_GLES))

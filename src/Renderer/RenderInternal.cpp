// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Everton Fernando Patitucci da Silva

#include "../Renderer/RenderAPI.h"
#if defined(USE_GL3) || defined(USE_GLES)
#define RENDERER_INTERNAL
#include "../Renderer/RenderInternal.h"
#include "../Renderer/RenderBatch.h"
#include "Debug/RenderDebug.h"
#include "Debug/GLTextureTracker.h"
#include "../Utility/PerfMarker.h"
#include <assert.h>
#if defined(XUO_EMSC)
#include <emscripten/html5.h>

#endif
#include <string.h> // memcmp, memcpy
#include <stdlib.h> // malloc
#include <vector>
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
    atexit([] { g_render.shutdown = true; });
#if defined(XUO_EMSC)
    // the browser owns the canvas: create a WebGL2 context directly and bind
    // it as the current GL context (SDL's EGL path is not usable on web).
    // GLES3 entry points are available on a webgl2 context; the GLSL ES 1.00
    // shaders keep working.
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.majorVersion = 2; // WebGL2
    attrs.minorVersion = 0;
    attrs.stencil = 1;
    attrs.depth = 1;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE wctx =
        emscripten_webgl_create_context("#canvas", &attrs);
    if (wctx <= 0)
    {
        Error(Renderer, "emscripten: could not create a WebGL2 context (%d)", (int)wctx);
        return false;
    }
    emscripten_webgl_make_context_current(wctx);
    g_render.context = (SDL_GLContext)(uintptr_t)wctx;
#else
    win_gfx_context_attrbutes(true);
    auto context = SDL_GL_GetCurrentContext();
    if (!context)
    {
        win_gfx_context_attrbutes(true);
        context = SDL_GL_CreateContext(window);
    }
    // always bind: the GL state (GL.currentContext) is only set on MakeCurrent
    SDL_GL_MakeCurrent(window, context);
#endif

#if defined(USE_GLEW)
    int glewInitResult = glewInit();
    if (glewInitResult != 0)
    {
        SDL_GL_DeleteContext(SDL_GL_GetCurrentContext());
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

    // wrap glGenTextures/glDeleteTextures before any texture exists so every
    // name is tracked from birth (XUO_TRACK_TEXTURES=1 to enable reporting)
    GLTextureTracker_Install();

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
        SDL_GL_DeleteContext(SDL_GL_GetCurrentContext());
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
#if defined(XUO_EMSC)
    {
        char vlog[1024] = {};
        glGetShaderInfoLog(vs, sizeof(vlog), nullptr, vlog);
        fprintf(stderr, "VS DIAG: status=%d log='%s'\n", (int)status, vlog);
        fflush(stdout);
    }
#endif
    if (status == GL_FALSE)
    {
        glGetShaderInfoLog(vs, sizeof(msg), nullptr, msg);
        Error(Renderer, "vs compilation: %s", msg);
#if defined(XUO_EMSC)
        fprintf(stderr, "--- VS SOURCE ---\n%s\n---", g_vShader);
        fflush(stdout);
#endif
    }

    const auto ps = glCreateShader(GL_FRAGMENT_SHADER);
    GL_CHECK(glShaderSource(ps, 1, &g_pShader, nullptr));
    GL_CHECK(glCompileShader(ps));
    status = GL_TRUE;
    GL_CHECK(glGetShaderiv(ps, GL_COMPILE_STATUS, &status));
#if defined(XUO_EMSC)
    {
        char slog[1024] = {};
        glGetShaderInfoLog(ps, sizeof(slog), nullptr, slog);
        fprintf(stderr, "PS DIAG: status=%d log='%s' version_ok=%d\n",
               (int)status, slog,
               (int)(strstr(g_pShader, "#version 300 es") == g_pShader));
        fflush(stdout);
    }
#endif
    if (status == GL_FALSE)
    {
        glGetShaderInfoLog(ps, sizeof(msg), nullptr, msg);
        Error(Renderer, "ps compilation: %s", msg);
#if defined(XUO_EMSC)
        fprintf(stderr, "--- PS SOURCE ---\n%s\n---", g_pShader);
        fflush(stdout);
#endif
    }

    _pProg = glCreateProgram();
    GL_CHECK(glAttachShader(_pProg, vs));
    GL_CHECK(glAttachShader(_pProg, ps));
    GL_CHECK(glLinkProgram(_pProg));
    status = GL_TRUE;
    GL_CHECK(glGetProgramiv(_pProg, GL_LINK_STATUS, &status));
    if (status == GL_FALSE)
    {
        glGetProgramInfoLog(_pProg, sizeof(msg), nullptr, msg);
        Error(Renderer, "program link: %s", msg);
#if defined(XUO_EMSC)
        fprintf(stderr, "--- LINK FAILED ---\n");
        fflush(stdout);
#endif
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
#if defined(XUO_EMSC)
    // SDL never saw a MakeCurrent (the webgl context was made current via
    // emscripten_webgl_make_context_current), so SDL_GL_GetCurrentContext
    // reports null here and would wipe the handle the webgl branch stored
    if (g_render.context == nullptr)
        g_render.context = (SDL_GLContext)(uintptr_t)emscripten_webgl_get_current_context();
#else
    g_render.context = SDL_GL_GetCurrentContext();
#endif
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
    GLTextureTracker_OnFrame();
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
#if defined(XUO_EMSC)
    // with OFFSCREEN_FRAMEBUFFER the frame lives in a wasm-side framebuffer:
    // commit posts it to the visible canvas (proxied to the browser main
    // thread, which stays free to process it)
    emscripten_webgl_commit_frame();
#elif !defined(XUO_EMSC)
    SDL_GL_SwapWindow(g_render.window);
#endif
    // on web the canvas is composited by the browser after every frame
}
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
    // WebGL2/GLES: GL_BGRA and the *_REV packed types do not exist. The
    // incoming pixels are BGRA-ordered (A1BGR5 for the 16-bit path, matching
    // the desktop GL reading of the same data); expand/swizzle to RGBA bytes
    // and upload as GL_RGBA + GL_UNSIGNED_BYTE.
    const auto imgFormat = GL_RGBA;
    std::vector<uint8_t> converted;
    if (pixels != nullptr)
    {
        const size_t count = size_t(width) * height;
        if (pixelsFormat == TextureFormat_Unsigned_A1_BGR5)
        {
            // ARGB1555 (bit15 = alpha, 14-10 = R, 9-5 = G, 4-0 = B) - the
            // same field mapping the desktop GL_BGRA + 1_5_5_5_REV upload
            // produces. The RGBA8 format arrives web-ready (no swizzle).
            converted.resize(count * 4);
            const auto *src = static_cast<const uint16_t *>(pixels);
            for (size_t i = 0; i < count; ++i)
            {
                const uint32_t c = src[i];
                converted[i * 4 + 0] = uint8_t((((c >> 10) & 0x1f) * 255 + 15) / 31);
                converted[i * 4 + 1] = uint8_t((((c >> 5) & 0x1f) * 255 + 15) / 31);
                converted[i * 4 + 2] = uint8_t(((c & 0x1f) * 255 + 15) / 31);
                converted[i * 4 + 3] = (c & 0x8000) != 0 ? 0xff : 0x00;
            }
            pixels = converted.data();
        }
    }
    static GLenum s_pixelFormatToOGLFormat[] = {
        GL_UNSIGNED_BYTE, // TextureFormat_Unsigned_RGBA8
        GL_UNSIGNED_BYTE, // TextureFormat_Unsigned_A1_BGR5 (converted above)
    };
#endif // #if defined(USE_GL)

    texture_handle_t tex = RENDER_TEXTUREHANDLE_INVALID;

    GL_CHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    //GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
    GL_CHECK(glGenTextures(1, &tex));
    GLTextureTracker_OnBind("CreateTexture2D", tex);
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
    GLTextureTracker_OnBind("CreateFrameBuffer", texture);
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, texture));
#if defined(USE_GL)
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, nullptr);
#else
    GL_CHECK(glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
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

// layers of destroyed sprites: the texture GC (3s unused -> DestroySprite ->
// CGLTexture::Clear) releases them here so re-visited areas re-place into dead
// layers instead of growing forever; only the owning sprite can reference a
// layer, and it releases before any other sprite can be handed it, so a
// reused layer can never be sampled through a stale reference
static int s_artArrayFree[ART_ARRAY_MAX_LAYERS];
static int s_artArrayFreeCount = 0;

// function-local static (NOT a file-scope const): on wasm the env var is set
// inside main(), which runs after all namespace-scope initializers - a file
// static would silently read the pre-main environment and never enable
static bool artarray_log_enabled()
{
    static const bool enabled = getenv("XUO_ART_ARRAY_LOG") != nullptr;
    return enabled;
}

void Render_ArtArrayRelease(int layer)
{
    if (layer < 0 || layer >= s_artArrayUsed)
    {
        return;
    }
    if (s_artArrayFreeCount < (int)countof(s_artArrayFree))
    {
        s_artArrayFree[s_artArrayFreeCount++] = layer;
        if (artarray_log_enabled())
        {
            fprintf(stderr, "[artarray] release layer=%d free=%d used=%d\n", layer, s_artArrayFreeCount, s_artArrayUsed);
        }
    }
}

texture_handle_t Render_ArtArrayTexture()
{
    return s_artArrayTexture;
}

#if defined(XUO_EMSC)
// WebGL2 exposes no glCopyImageSubData (GLES3-only API, absent from the
// WebGL2 IDL and from emscripten's GL bindings - the link would leave the
// symbol unresolved and any call would abort at runtime). Emulate the two
// uses below (2D -> array layer, and array grow) with a framebuffer attach
// + copyTexSubImage, which is WebGL2 core. The build compiles against the
// GLES2 headers (gl2.h), so the two GLES3 entry points used here get local
// prototypes (same pattern as the glTexImage3D declaration in gfx.h) -
// emscripten resolves them from the WebGL2 bindings. Framebuffer and
// texture bindings are saved and restored around the copies: the callers
// rely on the real glCopyImageSubData being side-effect free.
extern "C" {
void glFramebufferTextureLayer(
    GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
void glCopyTexSubImage3D(
    GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x,
    GLint y, GLsizei width, GLsizei height);
}
static void webgl_CopyImageSubData(
    uint32_t srcName, GLenum srcTarget, int srcLevel, int srcX, int srcY, int srcZ,
    uint32_t dstName, GLenum dstTarget, int dstLevel, int dstX, int dstY, int dstZ,
    int width, int height, int depth)
{
    GLint savedFbo = 0;
    GL_CHECK(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &savedFbo));
    GLint savedActive = GL_TEXTURE0;
    GL_CHECK(glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActive));
    const GLenum dstBindTarget =
        (dstTarget == GL_TEXTURE_2D) ? GL_TEXTURE_2D : GL_TEXTURE_2D_ARRAY;
    const GLint dstBindingEnum =
        (dstBindTarget == GL_TEXTURE_2D) ? GL_TEXTURE_BINDING_2D : GL_TEXTURE_BINDING_2D_ARRAY;

    GL_CHECK(glActiveTexture(GL_TEXTURE0));
    GLint savedTex = 0;
    GL_CHECK(glGetIntegerv(dstBindingEnum, &savedTex));

    GLuint fbo = 0;
    GL_CHECK(glGenFramebuffers(1, &fbo));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
    GL_CHECK(glBindTexture(dstBindTarget, dstName));
    for (int i = 0; i < depth; ++i)
    {
        if (srcTarget == GL_TEXTURE_2D)
        {
            GL_CHECK(glFramebufferTexture2D(
                GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, srcName, srcLevel));
        }
        else
        {
            GL_CHECK(glFramebufferTextureLayer(
                GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, srcName, srcLevel, srcZ + i));
        }
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            continue;
        }
        if (dstTarget == GL_TEXTURE_2D)
        {
            GL_CHECK(glCopyTexSubImage2D(
                GL_TEXTURE_2D, dstLevel, dstX, dstY, srcX, srcY, width, height));
        }
        else
        {
            GL_CHECK(glCopyTexSubImage3D(
                GL_TEXTURE_2D_ARRAY, dstLevel, dstX, dstY, dstZ + i, srcX, srcY, width, height));
        }
    }
    GL_CHECK(glDeleteFramebuffers(1, &fbo));

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, savedFbo));
    GL_CHECK(glBindTexture(dstBindTarget, savedTex));
    GL_CHECK(glActiveTexture(savedActive));
}

// initialize every layer of the array through an FBO clear. WebGL2 textures
// allocated with null data are UNINITIALIZED, and the first copyTexSubImage
// into any layer then forces the browser to zero-fill the WHOLE level
// ("forcing the browser to clear it. This may be slow" / "incurring lazy
// initialization") - a multi-hundred-MB stall on real drivers at the worst
// possible moment (the first world draws, on the GL thread). Clearing each
// layer once here marks the texture initialized per the WebGL2
// lazy-initialization rules, so the copy emulation never triggers it, and
// the backing store is committed through small explicit clears instead of
// one driver-surprise allocation (SwiftShader serves lazily-backed zero
// pages, so this cost only shows on real GPUs).
static void webgl_ArtArrayInitLayers(uint32_t texture, int layers)
{
    if (texture == 0 || layers <= 0)
    {
        return;
    }
    GLint savedFbo = 0;
    GL_CHECK(glGetIntegerv(GL_FRAMEBUFFER_BINDING, &savedFbo));
    GLint savedActive = GL_TEXTURE0;
    GL_CHECK(glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActive));
    GLfloat savedClear[4] = {0, 0, 0, 0};
    GL_CHECK(glGetFloatv(GL_COLOR_CLEAR_VALUE, savedClear));
    GLboolean savedMask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    GL_CHECK(glGetBooleanv(GL_COLOR_WRITEMASK, savedMask));
    const GLboolean savedScissor = glIsEnabled(GL_SCISSOR_TEST);
    GL_CHECK(glDisable(GL_SCISSOR_TEST));
    GL_CHECK(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
    GL_CHECK(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));

    GLuint fbo = 0;
    GL_CHECK(glGenFramebuffers(1, &fbo));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
    for (int layer = 0; layer < layers; ++layer)
    {
        GL_CHECK(glFramebufferTextureLayer(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0, layer));
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            continue;
        }
        GL_CHECK(glClear(GL_COLOR_BUFFER_BIT));
    }
    GL_CHECK(glDeleteFramebuffers(1, &fbo));

    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, savedFbo));
    GL_CHECK(glColorMask(savedMask[0], savedMask[1], savedMask[2], savedMask[3]));
    GL_CHECK(glClearColor(savedClear[0], savedClear[1], savedClear[2], savedClear[3]));
    if (savedScissor)
    {
        GL_CHECK(glEnable(GL_SCISSOR_TEST));
    }
    GL_CHECK(glActiveTexture(savedActive));
}

// WebGL2 reports texture allocation failures only through glGetError (GL_CHECK
// is compiled out in release): a dead array would leave every sprite and land
// tile unrendered (black world) while the client keeps running, so callers
// fall back to individual textures instead
static bool webgl_ArtArrayAllocOk()
{
    glGetError(); // flush any STALE pending error (release builds consume no
                  // errors anywhere else): only this alloc's result must count,
                  // an unrelated old error would permanently kill the array
    return glGetError() == GL_NO_ERROR;
}
#define glCopyImageSubData webgl_CopyImageSubData
#endif // XUO_EMSC

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
#if defined(XUO_EMSC)
        // a real driver must commit the array's backing store up front:
        // 1024 layers of 256x256 RGBA8 is ~256MB of VRAM at the FIRST world
        // draw. Start small and let the grow path below scale on demand;
        // SwiftShader (headless) serves lazily-backed zero pages, so this
        // cost is invisible there and only hits real GPUs
        const int initialLayers = 256;
#else
        const int initialLayers = ART_ARRAY_INITIAL_LAYERS;
#endif
        s_artArrayCap =
            (initialLayers < maxLayers) ? initialLayers : maxLayers;
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
            s_artArrayCap, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
#if defined(XUO_EMSC)
        if (!webgl_ArtArrayAllocOk())
        {
            GL_CHECK(glDeleteTextures(1, &s_artArrayTexture));
            s_artArrayTexture = 0;
            s_artArrayDisabled = true;
            Warning(
                Renderer,
                "art texture array allocation failed, sprites fall back to individual textures");
            return place;
        }
        webgl_ArtArrayInitLayers(s_artArrayTexture, s_artArrayCap);
#endif
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CHECK(glActiveTexture(GL_TEXTURE0));
    }

    if (s_artArrayFreeCount == 0 && s_artArrayUsed >= s_artArrayCap)
    {
        // grow the array (up to the GL limit) by recreating and copying
        if (s_artArrayCap >= ART_ARRAY_MAX_LAYERS)
        {
            // array full: this sprite keeps its individual texture (and the
            // batcher keys it separately); already-placed sprites stay merged
            static bool warnedFull = false;
            if (!warnedFull)
            {
                warnedFull = true;
                Warning(
                    Renderer,
                    "art texture array is full, new sprites fall back to individual textures");
            }
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
            newCap, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr));
#if defined(XUO_EMSC)
        if (!webgl_ArtArrayAllocOk())
        {
            // keep the old array: already-placed sprites stay merged, only
            // the overflow falls back to individual textures
            GL_CHECK(glDeleteTextures(1, &newArray));
            GL_CHECK(glActiveTexture(GL_TEXTURE0));
            s_artArrayDisabled = true;
            Warning(
                Renderer,
                "art texture array grow failed (%d layers), new sprites fall back to individual textures",
                newCap);
            return place;
        }
        webgl_ArtArrayInitLayers(newArray, newCap);
#endif
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL_CHECK(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
        GL_CHECK(glCopyImageSubData(
            s_artArrayTexture, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0,
            newArray, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, ART_ARRAY_LAYER_SIZE,
            ART_ARRAY_LAYER_SIZE, s_artArrayUsed));

        GL_CHECK(glDeleteTextures(1, &s_artArrayTexture));
        s_artArrayTexture = newArray;
        s_artArrayCap = newCap;
        if (artarray_log_enabled())
        {
            fprintf(stderr, "[artarray] grow cap=%d used=%d\n", s_artArrayCap, s_artArrayUsed);
        }

        GL_CHECK(glActiveTexture(GL_TEXTURE0));
    }

    // prefer a layer freed by a destroyed sprite: steady state stays at the
    // working-set size instead of growing with the session
    int layer;
    bool reused;
    if (s_artArrayFreeCount > 0)
    {
        layer = s_artArrayFree[--s_artArrayFreeCount];
        reused = true;
    }
    else
    {
        layer = s_artArrayUsed++;
        reused = false;
    }
    if (artarray_log_enabled())
    {
        fprintf(
            stderr,
            "[artarray] place layer=%d (%s) tex=%u %ux%u free=%d used=%d\n",
            layer,
            reused ? "reused" : "new",
            srcTexture,
            width,
            height,
            s_artArrayFreeCount,
            s_artArrayUsed);
    }
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

#endif // #if defined(USE_GL3) || defined(USE_GLES)

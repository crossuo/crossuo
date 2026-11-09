// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "gfx.h"
#include <assert.h>

#include "gfx.h"

// The GL entry-point table declared in gfx.h lives here: the renderer resolves
// it at startup and every unit shares this one definition.
#if defined(USE_GLES)
PFNGLACTIVETEXTUREPROC _xuo_glActiveTexture;
PFNGLATTACHSHADERPROC _xuo_glAttachShader;
PFNGLBINDBUFFERPROC _xuo_glBindBuffer;
PFNGLBINDFRAMEBUFFERPROC _xuo_glBindFramebuffer;
PFNGLBINDTEXTUREPROC _xuo_glBindTexture;
PFNGLBINDVERTEXARRAYPROC _xuo_glBindVertexArray;
PFNGLBLENDEQUATIONPROC _xuo_glBlendEquation;
PFNGLBLENDFUNCPROC _xuo_glBlendFunc;
PFNGLBUFFERDATAPROC _xuo_glBufferData;
PFNGLBUFFERSUBDATAPROC _xuo_glBufferSubData;
PFNGLCHECKFRAMEBUFFERSTATUSPROC _xuo_glCheckFramebufferStatus;
PFNGLCLEARPROC _xuo_glClear;
PFNGLCLEARCOLORPROC _xuo_glClearColor;
PFNGLCLEARDEPTHFPROC _xuo_glClearDepthf;
PFNGLCLEARSTENCILPROC _xuo_glClearStencil;
PFNGLCOLORMASKPROC _xuo_glColorMask;
PFNGLCOMPILESHADERPROC _xuo_glCompileShader;
PFNGLCREATEPROGRAMPROC _xuo_glCreateProgram;
PFNGLCREATESHADERPROC _xuo_glCreateShader;
PFNGLDELETEBUFFERSPROC _xuo_glDeleteBuffers;
PFNGLDELETEFRAMEBUFFERSPROC _xuo_glDeleteFramebuffers;
PFNGLDELETEPROGRAMPROC _xuo_glDeleteProgram;
PFNGLDELETESHADERPROC _xuo_glDeleteShader;
PFNGLDELETETEXTURESPROC _xuo_glDeleteTextures;
PFNGLDELETEVERTEXARRAYSPROC _xuo_glDeleteVertexArrays;
PFNGLDEPTHFUNCPROC _xuo_glDepthFunc;
PFNGLDISABLEPROC _xuo_glDisable;
PFNGLENABLEPROC _xuo_glEnable;
PFNGLENABLEVERTEXATTRIBARRAYPROC _xuo_glEnableVertexAttribArray;
PFNGLFRAMEBUFFERTEXTURE2DPROC _xuo_glFramebufferTexture2D;
PFNGLFRAMEBUFFERTEXTURELAYERPROC _xuo_glFramebufferTextureLayer;
PFNGLGENBUFFERSPROC _xuo_glGenBuffers;
PFNGLGENFRAMEBUFFERSPROC _xuo_glGenFramebuffers;
PFNGLGENTEXTURESPROC _xuo_glGenTextures;
PFNGLGENVERTEXARRAYSPROC _xuo_glGenVertexArrays;
PFNGLGENERATEMIPMAPPROC _xuo_glGenerateMipmap;
PFNGLGETATTRIBLOCATIONPROC _xuo_glGetAttribLocation;
PFNGLGETBOOLEANVPROC _xuo_glGetBooleanv;
PFNGLGETERRORPROC _xuo_glGetError;
PFNGLGETFLOATVPROC _xuo_glGetFloatv;
PFNGLGETINTEGERVPROC _xuo_glGetIntegerv;
PFNGLGETPROGRAMINFOLOGPROC _xuo_glGetProgramInfoLog;
PFNGLGETPROGRAMIVPROC _xuo_glGetProgramiv;
PFNGLGETSHADERINFOLOGPROC _xuo_glGetShaderInfoLog;
PFNGLGETSHADERIVPROC _xuo_glGetShaderiv;
PFNGLGETSTRINGPROC _xuo_glGetString;
PFNGLGETUNIFORMLOCATIONPROC _xuo_glGetUniformLocation;
PFNGLISTEXTUREPROC _xuo_glIsTexture;
PFNGLISENABLEDPROC _xuo_glIsEnabled;
PFNGLLINKPROGRAMPROC _xuo_glLinkProgram;
PFNGLPIXELSTOREIPROC _xuo_glPixelStorei;
PFNGLREADPIXELSPROC _xuo_glReadPixels;
PFNGLSCISSORPROC _xuo_glScissor;
PFNGLSHADERSOURCEPROC _xuo_glShaderSource;
PFNGLSTENCILFUNCPROC _xuo_glStencilFunc;
PFNGLSTENCILMASKPROC _xuo_glStencilMask;
PFNGLSTENCILOPPROC _xuo_glStencilOp;
PFNGLTEXIMAGE2DPROC _xuo_glTexImage2D;
PFNGLTEXPARAMETERIPROC _xuo_glTexParameteri;
PFNGLUNIFORM1FPROC _xuo_glUniform1f;
PFNGLUNIFORM1FVPROC _xuo_glUniform1fv;
PFNGLUNIFORM1IPROC _xuo_glUniform1i;
PFNGLUNIFORM2FPROC _xuo_glUniform2f;
PFNGLUNIFORM3FPROC _xuo_glUniform3f;
PFNGLUNIFORMMATRIX4FVPROC _xuo_glUniformMatrix4fv;
PFNGLUSEPROGRAMPROC _xuo_glUseProgram;
PFNGLVALIDATEPROGRAMPROC _xuo_glValidateProgram;
PFNGLVERTEXATTRIBPOINTERPROC _xuo_glVertexAttribPointer;
PFNGLVIEWPORTPROC _xuo_glViewport;
PFNGLDRAWARRAYSPROC _xuo_glDrawArrays;
PFNGLDRAWELEMENTSPROC _xuo_glDrawElements;
#if defined(XUO_GLES_DESKTOP) || defined(XUO_EMSC) || defined(XUO_GL_RUNTIME)
// ES3-only entry points: present on the desktop-GLES and web builds, and in
// the runtime (AUTO) build where the same table serves both context flavours
PFNGLTEXIMAGE3DPROC _xuo_glTexImage3D;
PFNGLCOPYIMAGESUBDATAPROC _xuo_glCopyImageSubData;
PFNGLCOPYTEXSUBIMAGE2DPROC _xuo_glCopyTexSubImage2D;
PFNGLCOPYTEXSUBIMAGE3DPROC _xuo_glCopyTexSubImage3D;
PFNGLPUSHDEBUGGROUPPROC _xuo_glPushDebugGroup;
PFNGLPOPDEBUGGROUPPROC _xuo_glPopDebugGroup;
PFNGLDEBUGMESSAGECALLBACKPROC _xuo_glDebugMessageCallback;
PFNGLDEBUGMESSAGECONTROLPROC _xuo_glDebugMessageControl;
#endif // XUO_GLES_DESKTOP or emscripten
int _xuo_missing_count = 0;
const char *_xuo_missing_first = 0;
char _xuo_missing_names[512] = { 0 };
#endif // USE_GLES

#define SOKOL_IMPL
#include "sokol_gfx.h"

#define SOKOL_GL_IMPL
#include "sokol_gl.h"

#define FONTSTASH_IMPLEMENTATION
#include "fontstash.h"

#define SOKOL_FONTSTASH_IMPL
#include "sokol_fontstash.h"

#if defined(USE_GL3W) && !defined(XUO_GL_RUNTIME)
#define GL3W_IMPLEMENTATION
#include "GL/gl3w.h"
#endif
// The automatic build only borrows gl3w's prototypes (through gfx.h) and
// resolves everything itself with SDL: compiling gl3w's implementation would
// drag in glXGetProcAddress, i.e. a hard libGL dependency the ES-only targets
// do not have.

#if defined(USE_DX11)
#include <SDL3/SDL_system.h>
#include <d3d11.h>
ID3D11Device *g_pd3dDevice = nullptr;
ID3D11DeviceContext *g_pd3dDeviceContext = nullptr;
IDXGISwapChain *g_pSwapChain = nullptr;
ID3D11RenderTargetView *g_mainRenderTargetView = nullptr;
#endif

void win_gfx_context_attrbutes(int enableDebug)
{
#if defined(USE_GL) || defined(USE_GLES)
#if defined(USE_GLES)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    #if defined(XUO_GLES_DESKTOP) && !defined(XUO_EMSC)
    // X11's native GL path is GLX, and GLX only ever hands out desktop GL
    // contexts. An ES profile request therefore has to go through EGL, whose
    // library set (libEGL/libGLESv2) is what ES-only stacks like Panfrost
    // actually ship - there is no libGL.so.1 to find there at all. SDL reads
    // this hint once at video init, so it must be set before SDL_Init*, and
    // it sticks for every window SDL creates afterwards.
    SDL_SetHint(SDL_HINT_VIDEO_FORCE_EGL, "1");
    #endif
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
#if !defined(USE_GL2)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, GFX_GL_MAJOR);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, GFX_GL_MINOR);
#endif // !defined(USE_GL2)
#if defined(XUO_GLES_DESKTOP) && !defined(XUO_EMSC)
    // ES3 has no debug-context flag in EGL; the debug output is opted into
    // after creation through KHR_debug when the driver advertises it, so do
    // not let a debug request cost the context
    (void)enableDebug;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
#elif defined(__APPLE__)
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
#else
    if (enableDebug)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    }
    else
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    }
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
#endif
}

int win_init(win_context *ctx)
{
    assert(ctx != 0);
    assert(ctx->title != 0);
    assert(ctx->width != 0);
    assert(ctx->height != 0);

    // SDL3 returns true on success (SDL2 returned 0)
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

#if defined(XUO_GL_RUNTIME)
    // the launcher/asset-viewer window must run on the SAME GL flavour as the
    // game: its context is created here on its own window, while the ImGui
    // GLSL version is shared with the game (see ui.cpp). A mismatched profile
    // compiles the wrong shader dialect on it - observed as a driver crash in
    // the GL thread (game on desktop GL, window requesting ES) - right after
    // the launcher initializes.
    glctx_apply_attributes(glctx_flavor_get(), ctx->debug != 0);
#else
    win_gfx_context_attrbutes(ctx->debug);
#endif

    const SDL_WindowFlags window_flags =
        (SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    // SDL3: no explicit position (centering is the default)
    SDL_Window *window = SDL_CreateWindow(
        ctx->title,
        ctx->width,
        ctx->height,
        window_flags);

    if (ctx->icon)
    {
        // SDL3 surfaces carry an explicit pixel format instead of bpp+masks
        const SDL_PixelFormat icon_format = (ctx->icon->bytes_per_pixel == 4)
                                                ? SDL_PIXELFORMAT_RGBA32
                                                : SDL_PIXELFORMAT_RGB24;
        SDL_Surface *icon = SDL_CreateSurfaceFrom(
            ctx->icon->width,
            ctx->icon->height,
            icon_format,
            (void *)ctx->icon->pixel_data,
            ctx->icon->bytes_per_pixel * ctx->icon->width);
        SDL_SetWindowIcon(window, icon);
        SDL_DestroySurface(icon);
    }

#if defined(USE_GL) || defined(USE_GLES)
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    ctx->window = window;
    ctx->context = gl_context;
    SDL_GL_SetSwapInterval(ctx->vsync);
#if defined(USE_GLEW)
    const int res = glewInit();
    if (res != GLEW_OK)
    {
        fprintf(stderr, "could not initialize opengl: %s\n", glewGetErrorString(res));
        return -2;
    }
#elif defined(USE_GL3W) && !defined(XUO_GL_RUNTIME)
    if (gl3wInit() != 0)
    {
        fprintf(stderr, "could not initialize opengl\n");
        return -2;
    }
#endif
#if defined(XUO_GL_RUNTIME)
    // resolve against the context that is current now; the launcher window
    // has its own context, so this has to run per window
    gfx_gl_load_proc_address();
#endif
#if GFX_GL_MAJOR >= 3
    {
        int major, minor;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        fprintf(stdout, "OpenGL %d.%d v(%s)\n", major, minor, glGetString(GL_VERSION));
    }
#else
    {
        fprintf(stdout, "OpenGL v(%s)\n", glGetString(GL_VERSION));
    }
#endif
#endif

#if defined(USE_DX11)
    // see https://github.com/floooh/sokol-samples/blob/master/d3d11/d3d11entry.c
    desc.d3d11_device = 0;         // ID3D11Device*
    desc.d3d11_device_context = 0; // ID3D11DeviceContext*
    desc.d3d11_render_target_view_cb =
        0; // ID3D11RenderTargetView* render_target_view (ID3D11Texture2D* render_target)
    desc.d3d11_depth_stencil_view_cb =
        0; // ID3D11DepthStencilView* depth_stencil_view (ID3D11Texture2D* depth_stencil_buffer)
#endif

    return 0;
}

void win_shutdown(win_context *ctx)
{
    assert(ctx != 0);
#if defined(USE_GL) || defined(USE_GLES)
    SDL_GL_DestroyContext(ctx->context);
#elif defined(USE_DX11)
    // ui_shutdown
#endif
    SDL_DestroyWindow(ctx->window);
    SDL_Quit();

    memset(ctx, 0, sizeof(win_context));
}

void win_flip(win_context *ctx)
{
    assert(ctx != 0);
#if defined(USE_GL) || defined(USE_GLES)
    SDL_GL_SwapWindow(ctx->window);
#elif defined(USE_DX11)
    // ui
#endif
}

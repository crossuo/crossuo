// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "gfx.h"
#include <assert.h>

#include "gfx.h"
#define SOKOL_IMPL
#include "sokol_gfx.h"

#define SOKOL_GL_IMPL
#include "sokol_gl.h"

#define FONTSTASH_IMPLEMENTATION
#include "fontstash.h"

#define SOKOL_FONTSTASH_IMPL
#include "sokol_fontstash.h"

#if defined(USE_GL3W)
#define GL3W_IMPLEMENTATION
#include "GL/gl3w.h"
#endif

#if defined(USE_DX11)
#include <SDL_syswm.h>
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
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
#if !defined(USE_GL2)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, GFX_GL_MAJOR);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, GFX_GL_MINOR);
#endif // !defined(USE_GL2)
#if defined(__APPLE__)
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

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    const SDL_WindowFlags window_flags =
        (SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window *window = SDL_CreateWindow(
        ctx->title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        ctx->width,
        ctx->height,
        window_flags);

    if (ctx->icon)
    {
        uint32_t rmask, gmask, bmask, amask;
    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
        int shift = (ctx->icon->bytes_per_pixel == 3) ? 8 : 0;
        rmask = 0xff000000 >> shift;
        gmask = 0x00ff0000 >> shift;
        bmask = 0x0000ff00 >> shift;
        amask = 0x000000ff >> shift;
    #else
        rmask = 0x000000ff;
        gmask = 0x0000ff00;
        bmask = 0x00ff0000;
        amask = (ctx->icon->bytes_per_pixel == 3) ? 0 : 0xff000000;
    #endif

        SDL_Surface *icon = SDL_CreateRGBSurfaceFrom(
            (void *)ctx->icon->pixel_data,
            ctx->icon->width,
            ctx->icon->height,
            ctx->icon->bytes_per_pixel * 8,
            ctx->icon->bytes_per_pixel * ctx->icon->width,
            rmask,
            gmask,
            bmask,
            amask);
        SDL_SetWindowIcon(window, icon);
        SDL_FreeSurface(icon);
    }

    win_gfx_context_attrbutes(ctx->debug);
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
#elif defined(USE_GL3W)
    if (gl3wInit() != 0)
    {
        fprintf(stderr, "could not initialize opengl\n");
        return -2;
    }
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
    SDL_GL_DeleteContext(ctx->context);
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

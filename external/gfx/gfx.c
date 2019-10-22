// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

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

#if defined(USE_GL)
	if (ctx->debug)
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	}
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, GFX_GL_MAJOR);
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, GFX_GL_MINOR);
#if __APPLE__
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
#endif // SOKOL_GLES
    const SDL_WindowFlags window_flags =
        (SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window *window = SDL_CreateWindow(
        ctx->title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ctx->width, ctx->height, window_flags);
#if defined(USE_GL)
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    ctx->window = window;
    ctx->context = gl_context;
    SDL_GL_SetSwapInterval(ctx->vsync);
#if defined(USE_GLEW)
    if (glewInit() != GLEW_OK)
    {
        fprintf(stderr, "could not initialize opengl\n");
        return -2;
    }
#else
    if (gl3wInit() != 0)
    {
        fprintf(stderr, "could not initialize opengl\n");
        return -2;
    }
#endif
    {
        int major, minor;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        fprintf(stdout, "OpenGL %d.%d v(%s)\n", major, minor, glGetString(GL_VERSION));
    }
#endif

    return 0;
}

void win_shutdown(win_context *ctx)
{
    assert(ctx != 0);
#if defined(USE_GL)
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
#if defined(USE_GL)
    SDL_GL_SwapWindow(ctx->window);
#elif defined(USE_DX11)
    // ui
#endif
}

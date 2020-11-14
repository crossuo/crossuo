// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#ifndef EXTERNAL_GFX_H
#define EXTERNAL_GFX_H


#define USE_GL3


#if defined(USE_GL3)
#define SOKOL_GLCORE33
#define SOKOL_GLES3
#define USE_GL3W
#define USE_GL
#define GFX_GL_MAJOR 3
#if defined(__APPLE__)
#define GFX_GL_MINOR 2
#else
#define GFX_GL_MINOR 0
#endif
#define IMGUI_IMPL_OPENGL_LOADER_GL3W
#elif defined(USE_GL2)
#define SOKOL_GLES2
#define USE_GLEW
#define USE_GL
#define GFX_GL_MAJOR 2
#define GFX_GL_MINOR 2
#define IMGUI_IMPL_OPENGL_LOADER_GLEW
#elif defined(USE_DX11)
#define SOKOL_D3D11
#elif defined(USE_METAL)
// SDL+Metal: https://gist.github.com/slime73/12284a8299be857d2581
#define SOKOL_METAL
#else
#define SOKOL_DUMMY_BACKEND
#endif

#define SOKOL_TRACE_HOOKS // sokol_gfx_imgui debugging helper


#if defined(USE_GLEW)


#elif defined(USE_GL3W) // USE_GLEW

#include "GL/gl3w.h"

#endif // USE_GLEW // USE_GL3W


#if defined(USE_GL)

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#elif defined(__linux__)
#include <GL/gl.h>
#endif // __APPLE__ // __linux__

#endif // USE_GL

#if defined(XUO_LOCAL_HEADERS)
#include "SDL.h"
#else
#include <SDL.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct win_icon_data_
{
    unsigned int width;
    unsigned int height;
    unsigned int bytes_per_pixel; /* 3:RGB, 4:RGBA */
    unsigned char pixel_data[120 * 120 * 4 + 1];
} win_icon_data;

typedef struct win_context_
{
    const char *title;
    SDL_Window *window;
    const win_icon_data *icon;
    void *sg_default_desc;
    void *sg_default_shader_desc;
    void *context;
    const char *inifile;
    int width;
    int height;
    int vsync;
    int debug;
} win_context;

int win_init(win_context *ctx);
void win_shutdown(win_context *ctx);
void win_flip(win_context *ctx);

#ifdef __cplusplus
}
#endif

#endif // #ifndef EXTERNAL_GFX_H

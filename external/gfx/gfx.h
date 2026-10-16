// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#ifndef EXTERNAL_GFX_H
#define EXTERNAL_GFX_H

#include <stdlib.h> // getenv (gfx_gl_validation_enabled)

#if defined(USE_GL3)
    #define USE_GL
    #define USE_GL3W
    #define SOKOL_GLCORE33
    #define SOKOL_GLES3
    #define GFX_GL_MAJOR 3
    #define GFX_GL_MINOR 2
    #if defined(__APPLE__)
        #define GL_SHADER_VERSION "150"
    #else // #if defined(__APPLE__)
        #define GL_SHADER_VERSION "330"
    #endif // #else // #if defined(__APPLE__)
    #define GL_SHADER_ATTRIBUTE(x) "layout (location = " #x " ) in "
    #define GL_SHADER_IN "in "
    #define GL_SHADER_OUT "out "
#elif defined(USE_GL1)
    #define USE_GL
    #define USE_GLEW
    #define SOKOL_DUMMY_BACKEND
    #define GL_SHADER_VERSION "120"
    #define GL_SHADER_ATTRIBUTE(x) "attribute "
    #define GL_SHADER_IN "varying "
    #define GL_SHADER_OUT "varying "
    #define GFX_GL_MAJOR 1
    #define GFX_GL_MINOR 1
#elif defined(USE_GL2) // #elif defined(USE_GL1)
    #define USE_GL
    #define USE_GLEW
    #define SOKOL_GLES2
    #define GL_SHADER_VERSION "120"
    #define GL_SHADER_ATTRIBUTE(x) "in "
    #define GL_SHADER_IN "varying "
    #define GL_SHADER_OUT "varying "
    #define GFX_GL_MAJOR 2
    #define GFX_GL_MINOR 2
#elif defined(USE_GLES2) // #if defined(USE_GL2)
    // the build system passes -DUSE_GLES for the src renderer gates (it must
    // exist before this header is included); only define it here when absent
    // (a valueless -D is seen as '=1', which would otherwise conflict with the
    // empty definition and warn on every user of this header)
    #ifndef USE_GLES
        #define USE_GLES
    #endif
    #define SOKOL_GLES2
    #define GL_SHADER_VERSION "100"
    #define GL_SHADER_ATTRIBUTE(x) "in "
    #define GL_SHADER_IN "varying "
    #define GL_SHADER_OUT "varying "
    #define GFX_GL_MAJOR 2
    #define GFX_GL_MINOR 1
    #if defined(XUO_EMSC)
        // webgl2: GLES3 context (glTexImage3D/glCopyImageSubData) and GLSL
        // ES 3.00 shaders (the ES 1.00 attribute/varying syntax above is not
        // valid on a webgl2 context)
        #undef GFX_GL_MAJOR
        #undef GFX_GL_MINOR
        #undef GL_SHADER_VERSION
        #undef GL_SHADER_ATTRIBUTE
        #undef GL_SHADER_IN
        #undef GL_SHADER_OUT
        #define GFX_GL_MAJOR 3
        #define GFX_GL_MINOR 0
        #define GL_SHADER_VERSION "300 es"
        #define GL_SHADER_ATTRIBUTE(x) "layout (location = " #x " ) in "
        #define GL_SHADER_IN "in "
        #define GL_SHADER_OUT "out "
    #endif
    #define IMGUI_IMPL_OPENGL_ES2
    //#define IMGUI_IMPL_OPENGL_ES3
    #if defined(XUO_EMSC)
    // webgl2 (-sUSE_WEBGL2=1): GLES3 entry points the GLES2 headers omit,
    // plus the desktop constants the renderer uses (GL_BGRA etc. are
    // compile-time shims - check runtime support when touching those paths)
    #ifndef GL_MAX_ARRAY_TEXTURE_LAYERS
        #define GL_MAX_ARRAY_TEXTURE_LAYERS 0x88FF
    #endif
    #ifndef GL_TEXTURE_BINDING_2D_ARRAY
        #define GL_TEXTURE_BINDING_2D_ARRAY 0x8C1D
    #endif
    #ifndef GL_TEXTURE_BINDING_3D
        #define GL_TEXTURE_BINDING_3D 0x806A
    #endif
    #ifndef GL_BGRA
        #define GL_BGRA 0x80E1
    #endif
    #ifndef GL_UNSIGNED_SHORT_1_5_5_5_REV
        #define GL_UNSIGNED_SHORT_1_5_5_5_REV 0x8366
    #endif
    #ifndef GL_MAJOR_VERSION
        #define GL_MAJOR_VERSION 0x821B
    #endif
    #ifndef GL_MINOR_VERSION
        #define GL_MINOR_VERSION 0x821C
    #endif
    #ifdef __cplusplus
    extern "C" {
    #endif
    void glTexImage3D(
        unsigned target, int level, int internalformat, unsigned width, unsigned height,
        unsigned depth, int border, unsigned format, unsigned type, const void *data);
    void glCopyImageSubData(
        unsigned srcName, unsigned srcTarget, int srcLevel, int srcX, int srcY, int srcZ,
        unsigned dstName, unsigned dstTarget, int dstLevel, int dstX, int dstY, int dstZ,
        unsigned width, unsigned height, unsigned depth);
    #ifdef __cplusplus
    }
    #endif
#endif // GLES2 branch
#elif defined(USE_DX11) // #elif defined(USE_GL2)
    #define SOKOL_D3D11
#elif defined(USE_METAL) // #elif defined(USE_DX11)
    // SDL+Metal: https://gist.github.com/slime73/12284a8299be857d2581
    #define SOKOL_METAL
#else // #elif defined(USE_METAL)
    #define SOKOL_DUMMY_BACKEND
#endif // USE_GL3

#define SOKOL_TRACE_HOOKS // sokol_gfx_imgui debugging helper

#if defined(USE_GLEW)
    #define IMGUI_IMPL_OPENGL_LOADER_GLEW
    #define NO_SDL_GLEXT
    #include "GL/glew.h"
    #if defined(_MSC_VER)
        #include "GL/wglew.h"
    #endif // #else // #if defined(_MSC_VER)
#elif defined(USE_GL3W) // #if defined(USE_GLEW)
    #define IMGUI_IMPL_OPENGL_LOADER_GL3W
    #include "GL/gl3w.h"
#endif // #elif defined(USE_GL3W) // #if defined(USE_GLEW)

#if defined(USE_GL)
    #if defined(__APPLE__)
        #define GL_SILENCE_DEPRECATION
        #include <OpenGL/gl.h>
    #elif defined(__linux__) // #if defined(__APPLE__)
        #include <GL/gl.h>
    #endif // #elif defined(__linux__) // #if defined(__APPLE__)
    //#include <SDL_opengl.h>
#elif defined(USE_GLES) // #if defined(USE_GL)
    #if (USE_GLES2)
        //#define GL_GLEXT_PROTOTYPES
        //#include <GLES2/gl2.h>
        //#include <GLES2/gl2ext.h>
        #include <SDL_opengles2.h>
        #define __glew_h__ // HACK to get sokol_gfx use the right symbols
    #endif
#endif // #elif defined(USE_GLES) // #if defined(USE_GL)

#if defined(USE_GL) || defined(USE_GLES)

// optional hook: the renderer's texture-name tracker defines this to print the
// full creation/deletion story before a failed GL_CHECK aborts. Declared weak
// so this external header stays standalone (the tracker may not be linked).
// XUO_GL_NO_ABORT=1 turns the abort into a log-only error for a non-fatal run.
// C-compatible declaration: this header is also consumed by gfx.c
#if !defined(GLTEXTURE_TRACKER_ON_GL_ERROR_DECLARED)
#define GLTEXTURE_TRACKER_ON_GL_ERROR_DECLARED
#if defined(__cplusplus)
extern "C"
#endif
__attribute__((weak)) void GLTextureTracker_OnGLError(
    const char *statement, unsigned int error);
#endif

// GL error validation: glGetError after every call is a large CPU tax at
// ~10k calls/frame. It is ON for debug builds (XUO_DEBUG) and OFF for
// release. XUO_GL_VALIDATE=1 forces it on (validating a release build),
// XUO_GL_VALIDATE=0 forces it off (benchmarking a debug build).
static inline int gfx_gl_validation_enabled()
{
    static int cached = -1;
    if (cached == -1)
    {
        const char *v = getenv("XUO_GL_VALIDATE");
#if defined(XUO_DEBUG)
        cached = (v == NULL || v[0] != '0') ? 1 : 0;
#else
        cached = (v != NULL && v[0] != '0') ? 1 : 0;
#endif
    }
    return cached;
}

#define GL_CHECK_ATTRIB(x) \
    do { \
        if (x == -1) { \
            Error(Renderer, "Attib %s not found or removed from shader", TOSTRING(x)); \
            exit(-68); \
        } \
    } while(0)
#define GL_CHECK(statement) \
    do { \
        statement; \
        if (gfx_gl_validation_enabled()) { \
            const auto e = glGetError(); \
            if (e != GL_NO_ERROR) { \
                Error(Renderer, TOSTRING(statement) " returned error: 0x%04x", e); \
                if (GLTextureTracker_OnGLError) { \
                    GLTextureTracker_OnGLError(TOSTRING(statement), (unsigned int)e); \
                } \
                if (getenv("XUO_GL_NO_ABORT") == NULL) { \
                    abort(); \
                } \
            } \
        } \
    } while(0)
#define GL_CALL(statement) \
    do { \
        statement; \
        if (gfx_gl_validation_enabled()) { \
            const auto e = glGetError(); \
            if (e != GL_NO_ERROR) { \
                Warning(Renderer, TOSTRING(statement) " returned error: 0x%04x", e); \
            } \
        } \
    } while(0)
// GLES3/WebGL2 runtime support (emscripten): the array sampler needs an
// explicit precision in fragment shaders and texture2D does not exist in
// GLSL ES 3.00. Injected via the C preprocessor - GLSL-level #ifdefs cannot
// see C compiler defines.
#if defined(XUO_EMSC)
#define GL_SHADER_ES3_COMPAT "precision highp sampler2DArray;\n" "#define texture2D texture\n"
#else
#define GL_SHADER_ES3_COMPAT ""
#endif

#define GL_SHADER_HEADER \
    "#version " GL_SHADER_VERSION "\n" \
    GL_SHADER_ES3_COMPAT \
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n" \
    "precision highp float;\n" \
    "#else\n" \
    "precision mediump float;\n" \
    "#endif\n" \
    "out highp vec4 xuo_fragColor;\n"
#endif

#if (__cplusplus >= 201100)
#define OFFSETOF(_TYPE,_MEMBER)  offsetof(_TYPE, _MEMBER)
#else
#define OFFSETOF(_TYPE,_MEMBER)  ((size_t)&(((_TYPE*)0)->_MEMBER))
#endif

struct GenericVertex
{
    float pos[2];
    float z;          // isometric depth key for the world pass (0 elsewhere);
                      // read together with pos as a 3-component vertex attrib
    float uv[2];
    unsigned int col;
    float normal[3];  // Normal vector for 3D rendering (default [0,0,1] for 2D)
    float layer;
};

#if defined(XUO_LOCAL_HEADERS)
    #include "SDL.h"
#else // #if defined(XUO_LOCAL_HEADERS)
    #include <SDL.h>
#endif // #else // #if defined(XUO_LOCAL_HEADERS)

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
void win_gfx_context_attrbutes(int enableDebug);

#ifdef __cplusplus
}
#endif

#endif // #ifndef EXTERNAL_GFX_H

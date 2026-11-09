// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#ifndef EXTERNAL_GFX_H
#define EXTERNAL_GFX_H

#include <stdlib.h> // getenv (gfx_gl_validation_enabled)

#if defined(XUO_GL_RUNTIME)
    // One binary for both desktop GL 3.2 core and OpenGL ES 3 (glctx picks at
    // startup). The renderer's compile-time gates follow the ES3 family - it
    // is the stricter of the two (no deprecated entry points) - while the
    // headers come from the GL3 set, which is a superset and carries every
    // prototype the loader needs for either context.
    #define USE_GL
    #ifndef USE_GLES
        #define USE_GLES
    #endif
    // No gl3w here: its header rewrites every gl* name to a gl3w* global and
    // its implementation dlopens libGL. The runtime binary resolves through
    // SDL instead, using the glcorearb prototypes (the GL3 superset) below.
    // sokol's GL backend is not used by the client renderer.
    #define XUO_GL_RUNTIME_PROTOTYPES
    #define SOKOL_DUMMY_BACKEND
    #define GFX_GL_MAJOR 3
    #define GFX_GL_MINOR 2
    #if defined(__APPLE__)
        #define GL_SHADER_VERSION "150"
    #else
        #define GL_SHADER_VERSION "330"
    #endif
    #define GL_SHADER_ATTRIBUTE(x) "layout (location = " #x " ) in "
    #define GL_SHADER_IN "in "
    #define GL_SHADER_OUT "out "
#elif defined(USE_GL3)
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
    #if defined(XUO_EMSC) || defined(XUO_GLES_DESKTOP)
        // webgl2 and the native desktop GLES build: a GLES3 context
        // (glTexImage3D/glCopyImageSubData/VAOs) running GLSL ES 3.00 (the
        // ES 1.00 attribute/varying syntax above is not valid there)
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
    #if defined(XUO_GLES_DESKTOP)
    // the arrow is baked into this choice: ES3 contexts need the ES3 backend
    #define IMGUI_IMPL_OPENGL_ES3
    #else
    #define IMGUI_IMPL_OPENGL_ES2
    #endif
    //#define IMGUI_IMPL_OPENGL_ES3
    // webgl2 / native GLES2 devices: the desktop constants the renderer uses.
    // glTexImage3D / glCopyImageSubData are declared only where the headers
    // lack them; the desktop GLES build (XUO_GLES_DESKTOP) includes the real
    // glcorearb prototypes instead, so re-declaring them would clash
#if defined(XUO_EMSC) || defined(__arm__) || defined(__aarch64__) || defined(XUO_GLES_DESKTOP)
    #ifndef GL_TEXTURE_2D_ARRAY
        #define GL_TEXTURE_2D_ARRAY 0x8C1A
    #endif
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
    // glTexImage3D / glCopyImageSubData prototypes come from the glcorearb
    // package included below - no manual declarations needed
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

#if defined(XUO_GL_RUNTIME_PROTOTYPES)
    // The vendored package carries the whole GL 1.0-3.0 prototype set (the
    // superset both contexts need) and the khronos types the ES headers want.
    // Pretending gl2.h is already included keeps the system header - whose
    // signatures are macros rather than typedefs - out of the way.
    #define __gl2_h_
    #define GLCOREARB_PROTOTYPES 1
    #include "GL/glcorearb.h"
    // The khronos platform header ships with the ES dev packages; where none
    // exist (Windows, macOS, musl containers) glcorearb.h above already
    // provided every type/macro the loader needs, so it is optional.
    #if defined(__has_include)
    #if __has_include(<GLES2/gl2platform.h>)
    #include <GLES2/gl2platform.h>
    #endif
    #endif
#elif defined(USE_GL)
    #if defined(__APPLE__)
        #define GL_SILENCE_DEPRECATION
        #include <OpenGL/gl.h>
    #elif defined(__linux__) // #if defined(__APPLE__)
        #include <GL/gl.h>
    #endif // #elif defined(__linux__) // #if defined(__APPLE__)
    //#include <SDL3/SDL_opengl.h>
#elif defined(USE_GLES)
    // Every GLES build that uses the _xuo_ loader (webgl2 via emscripten,
    // the desktop GLES forced build, the AUTO runtime) sources its
    // prototypes from the vendored glcorearb package: the ES2 system
    // headers stop at ES 2.0, so ES3 typedefs (glTexImage3D,
    // glCopyImageSubData, debug output, ...) would be missing exactly
    // where the loader needs them.
    // Pretending gl2.h is already included keeps the Khronos system
    // header out: its signatures would be a second source of truth and
    // would pre-empt the package's versions of the same declarations.
    // gl2platform.h still comes in below for the khronos type/macro
    // definitions.
    #define __glew_h__ // HACK to get sokol_gfx use the right symbols
    #define __gl2_h_
    #define GLCOREARB_PROTOTYPES 1
    #include "GL/glcorearb.h"
    // same optional khronos platform include as the runtime branch above
    #if defined(__has_include)
    #if __has_include(<GLES2/gl2platform.h>)
    #include <GLES2/gl2platform.h>
    #endif
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
#if defined(XUO_EMSC) || defined(XUO_GLES_DESKTOP) || defined(XUO_GL_RUNTIME)
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

// Runtime GL flavour (desktop 3.2 core vs ES 3.0) and its capabilities. Every
// consumer of gfx.h gets it, so the renderer can query the context instead of
// testing build flags.
#include "glctx.h"

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
    #include "SDL3/SDL.h"
#else // #if defined(XUO_LOCAL_HEADERS)
    #include <SDL3/SDL.h>
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
    // SDL3: SDL_GLContext is a typed pointer (was void * in SDL2)
    SDL_GLContext context;
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

// Resolves every GL entry point the renderer uses through the windowing layer
// instead of a vendored loader. SDL knows which library actually backs the
// current context (libGL for GLX/WGL, libGLESv2 for EGL), so this is the only
// path that works on an ES-only stack where libGL.so.1/glXGetProcAddress do not
// exist. Call it with the context already current and BEFORE GLTextureTracker_
// Install() replaces glGenTextures/glDeleteTextures with its wrappers.
//
// GL entry points, resolved through SDL_GL_GetProcAddress at Render_Init().
//
// The pointers have EXTERNAL linkage on purpose. A file-local (`static`)
// pointer that is only written inside this header would let the optimizer
// prove the export unreachable in a unit that never calls the loader and
// delete the call site outright (observed as the call simply vanishing, then
// a jump through garbage). With external linkage the compiler must assume
// another unit can change it before each use.
//
// A single call resolves every entry point in one place; call sites keep the
// plain GL spelling through the macros below, so nothing links from libGL or
// libGLESv2 and the driver only has to hand out what it actually implements.
//
// Only the ES backend uses this. On desktop GL a prototype from the system
// headers is a real function, not an assignable lvalue; those builds keep
// GLEW (GL1) and gl3w (GL3).
#if defined(USE_GLES) || defined(XUO_GLES_DESKTOP) || defined(XUO_GL_RUNTIME)

extern PFNGLACTIVETEXTUREPROC _xuo_glActiveTexture;
extern PFNGLATTACHSHADERPROC _xuo_glAttachShader;
extern PFNGLBINDBUFFERPROC _xuo_glBindBuffer;
extern PFNGLBINDFRAMEBUFFERPROC _xuo_glBindFramebuffer;
extern PFNGLBINDTEXTUREPROC _xuo_glBindTexture;
extern PFNGLBINDVERTEXARRAYPROC _xuo_glBindVertexArray;
extern PFNGLBLENDEQUATIONPROC _xuo_glBlendEquation;
extern PFNGLBLENDFUNCPROC _xuo_glBlendFunc;
extern PFNGLBUFFERDATAPROC _xuo_glBufferData;
extern PFNGLBUFFERSUBDATAPROC _xuo_glBufferSubData;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC _xuo_glCheckFramebufferStatus;
extern PFNGLCLEARPROC _xuo_glClear;
extern PFNGLCLEARCOLORPROC _xuo_glClearColor;
extern PFNGLCLEARDEPTHFPROC _xuo_glClearDepthf;
extern PFNGLCLEARSTENCILPROC _xuo_glClearStencil;
extern PFNGLCOLORMASKPROC _xuo_glColorMask;
extern PFNGLCOMPILESHADERPROC _xuo_glCompileShader;
extern PFNGLCREATEPROGRAMPROC _xuo_glCreateProgram;
extern PFNGLCREATESHADERPROC _xuo_glCreateShader;
extern PFNGLDELETEBUFFERSPROC _xuo_glDeleteBuffers;
extern PFNGLDELETEFRAMEBUFFERSPROC _xuo_glDeleteFramebuffers;
extern PFNGLDELETEPROGRAMPROC _xuo_glDeleteProgram;
extern PFNGLDELETESHADERPROC _xuo_glDeleteShader;
extern PFNGLDELETETEXTURESPROC _xuo_glDeleteTextures;
extern PFNGLDELETEVERTEXARRAYSPROC _xuo_glDeleteVertexArrays;
extern PFNGLDEPTHFUNCPROC _xuo_glDepthFunc;
extern PFNGLDISABLEPROC _xuo_glDisable;
extern PFNGLENABLEPROC _xuo_glEnable;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC _xuo_glEnableVertexAttribArray;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC _xuo_glFramebufferTexture2D;
extern PFNGLFRAMEBUFFERTEXTURELAYERPROC _xuo_glFramebufferTextureLayer;
extern PFNGLGENBUFFERSPROC _xuo_glGenBuffers;
extern PFNGLGENFRAMEBUFFERSPROC _xuo_glGenFramebuffers;
extern PFNGLGENTEXTURESPROC _xuo_glGenTextures;
extern PFNGLGENVERTEXARRAYSPROC _xuo_glGenVertexArrays;
extern PFNGLGENERATEMIPMAPPROC _xuo_glGenerateMipmap;
extern PFNGLGETATTRIBLOCATIONPROC _xuo_glGetAttribLocation;
extern PFNGLGETBOOLEANVPROC _xuo_glGetBooleanv;
extern PFNGLGETERRORPROC _xuo_glGetError;
extern PFNGLGETFLOATVPROC _xuo_glGetFloatv;
extern PFNGLGETINTEGERVPROC _xuo_glGetIntegerv;
extern PFNGLGETPROGRAMINFOLOGPROC _xuo_glGetProgramInfoLog;
extern PFNGLGETPROGRAMIVPROC _xuo_glGetProgramiv;
extern PFNGLGETSHADERINFOLOGPROC _xuo_glGetShaderInfoLog;
extern PFNGLGETSHADERIVPROC _xuo_glGetShaderiv;
extern PFNGLGETSTRINGPROC _xuo_glGetString;
extern PFNGLGETUNIFORMLOCATIONPROC _xuo_glGetUniformLocation;
extern PFNGLISTEXTUREPROC _xuo_glIsTexture;
extern PFNGLISENABLEDPROC _xuo_glIsEnabled;
extern PFNGLLINKPROGRAMPROC _xuo_glLinkProgram;
extern PFNGLPIXELSTOREIPROC _xuo_glPixelStorei;
extern PFNGLREADPIXELSPROC _xuo_glReadPixels;
extern PFNGLSCISSORPROC _xuo_glScissor;
extern PFNGLSHADERSOURCEPROC _xuo_glShaderSource;
extern PFNGLSTENCILFUNCPROC _xuo_glStencilFunc;
extern PFNGLSTENCILMASKPROC _xuo_glStencilMask;
extern PFNGLSTENCILOPPROC _xuo_glStencilOp;
extern PFNGLTEXIMAGE2DPROC _xuo_glTexImage2D;
extern PFNGLTEXPARAMETERIPROC _xuo_glTexParameteri;
extern PFNGLUNIFORM1FPROC _xuo_glUniform1f;
extern PFNGLUNIFORM1FVPROC _xuo_glUniform1fv;
extern PFNGLUNIFORM1IPROC _xuo_glUniform1i;
extern PFNGLUNIFORM2FPROC _xuo_glUniform2f;
extern PFNGLUNIFORM3FPROC _xuo_glUniform3f;
extern PFNGLUNIFORMMATRIX4FVPROC _xuo_glUniformMatrix4fv;
extern PFNGLUSEPROGRAMPROC _xuo_glUseProgram;
extern PFNGLVALIDATEPROGRAMPROC _xuo_glValidateProgram;
extern PFNGLVERTEXATTRIBPOINTERPROC _xuo_glVertexAttribPointer;
extern PFNGLVIEWPORTPROC _xuo_glViewport;
extern PFNGLDRAWARRAYSPROC _xuo_glDrawArrays;
extern PFNGLDRAWELEMENTSPROC _xuo_glDrawElements;
extern PFNGLTEXIMAGE3DPROC _xuo_glTexImage3D;
extern PFNGLCOPYIMAGESUBDATAPROC _xuo_glCopyImageSubData;
extern PFNGLCOPYTEXSUBIMAGE2DPROC _xuo_glCopyTexSubImage2D;
extern PFNGLCOPYTEXSUBIMAGE3DPROC _xuo_glCopyTexSubImage3D;
extern PFNGLPUSHDEBUGGROUPPROC _xuo_glPushDebugGroup;
extern PFNGLPOPDEBUGGROUPPROC _xuo_glPopDebugGroup;
extern PFNGLDEBUGMESSAGECALLBACKPROC _xuo_glDebugMessageCallback;
extern PFNGLDEBUGMESSAGECONTROLPROC _xuo_glDebugMessageControl;

// Set by the load call so a partial driver can be reported at startup
// instead of crashing on the first unsupported call.
extern int _xuo_missing_count;
extern const char *_xuo_missing_first;
extern char _xuo_missing_names[512];

#define XUO_GL_MISSING(_NAME)                                                     \
    do                                                                            \
    {                                                                             \
        if (_xuo_missing_count == 0)                                              \
        {                                                                         \
            _xuo_missing_first = #_NAME;                                          \
        }                                                                         \
        if (_xuo_missing_count < 32)                                              \
        {                                                                         \
            const char *_n = #_NAME;                                              \
            size_t _len = strlen(_xuo_missing_names);                             \
            size_t _need = strlen(_n) + 2;                                        \
            if (_len + _need < sizeof(_xuo_missing_names))                        \
            {                                                                     \
                if (_len != 0)                                                    \
                {                                                                 \
                    _xuo_missing_names[_len++] = ' ';                             \
                }                                                                 \
                memcpy(_xuo_missing_names + _len, _n, _need - 1);                 \
                _xuo_missing_names[_len + _need - 1] = 0;                         \
            }                                                                     \
        }                                                                         \
        _xuo_missing_count++;                                                     \
    } while (0)

static inline void gfx_gl_load_proc_address_into(void)
{
    _xuo_glActiveTexture = (PFNGLACTIVETEXTUREPROC)SDL_GL_GetProcAddress("glActiveTexture"); if (_xuo_glActiveTexture == NULL) { XUO_GL_MISSING(glActiveTexture); }
    _xuo_glAttachShader = (PFNGLATTACHSHADERPROC)SDL_GL_GetProcAddress("glAttachShader"); if (_xuo_glAttachShader == NULL) { XUO_GL_MISSING(glAttachShader); }
    _xuo_glBindBuffer = (PFNGLBINDBUFFERPROC)SDL_GL_GetProcAddress("glBindBuffer"); if (_xuo_glBindBuffer == NULL) { XUO_GL_MISSING(glBindBuffer); }
    _xuo_glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)SDL_GL_GetProcAddress("glBindFramebuffer"); if (_xuo_glBindFramebuffer == NULL) { XUO_GL_MISSING(glBindFramebuffer); }
    _xuo_glBindTexture = (PFNGLBINDTEXTUREPROC)SDL_GL_GetProcAddress("glBindTexture"); if (_xuo_glBindTexture == NULL) { XUO_GL_MISSING(glBindTexture); }
    _xuo_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)SDL_GL_GetProcAddress("glBindVertexArray"); if (_xuo_glBindVertexArray == NULL) { XUO_GL_MISSING(glBindVertexArray); }
    _xuo_glBlendEquation = (PFNGLBLENDEQUATIONPROC)SDL_GL_GetProcAddress("glBlendEquation"); if (_xuo_glBlendEquation == NULL) { XUO_GL_MISSING(glBlendEquation); }
    _xuo_glBlendFunc = (PFNGLBLENDFUNCPROC)SDL_GL_GetProcAddress("glBlendFunc"); if (_xuo_glBlendFunc == NULL) { XUO_GL_MISSING(glBlendFunc); }
    _xuo_glBufferData = (PFNGLBUFFERDATAPROC)SDL_GL_GetProcAddress("glBufferData"); if (_xuo_glBufferData == NULL) { XUO_GL_MISSING(glBufferData); }
    _xuo_glBufferSubData = (PFNGLBUFFERSUBDATAPROC)SDL_GL_GetProcAddress("glBufferSubData"); if (_xuo_glBufferSubData == NULL) { XUO_GL_MISSING(glBufferSubData); }
    _xuo_glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)SDL_GL_GetProcAddress("glCheckFramebufferStatus"); if (_xuo_glCheckFramebufferStatus == NULL) { XUO_GL_MISSING(glCheckFramebufferStatus); }
    _xuo_glClear = (PFNGLCLEARPROC)SDL_GL_GetProcAddress("glClear"); if (_xuo_glClear == NULL) { XUO_GL_MISSING(glClear); }
    _xuo_glClearColor = (PFNGLCLEARCOLORPROC)SDL_GL_GetProcAddress("glClearColor"); if (_xuo_glClearColor == NULL) { XUO_GL_MISSING(glClearColor); }
    _xuo_glClearDepthf = (PFNGLCLEARDEPTHFPROC)SDL_GL_GetProcAddress("glClearDepthf"); if (_xuo_glClearDepthf == NULL) { XUO_GL_MISSING(glClearDepthf); }
    _xuo_glClearStencil = (PFNGLCLEARSTENCILPROC)SDL_GL_GetProcAddress("glClearStencil"); if (_xuo_glClearStencil == NULL) { XUO_GL_MISSING(glClearStencil); }
    _xuo_glColorMask = (PFNGLCOLORMASKPROC)SDL_GL_GetProcAddress("glColorMask"); if (_xuo_glColorMask == NULL) { XUO_GL_MISSING(glColorMask); }
    _xuo_glCompileShader = (PFNGLCOMPILESHADERPROC)SDL_GL_GetProcAddress("glCompileShader"); if (_xuo_glCompileShader == NULL) { XUO_GL_MISSING(glCompileShader); }
    _xuo_glCreateProgram = (PFNGLCREATEPROGRAMPROC)SDL_GL_GetProcAddress("glCreateProgram"); if (_xuo_glCreateProgram == NULL) { XUO_GL_MISSING(glCreateProgram); }
    _xuo_glCreateShader = (PFNGLCREATESHADERPROC)SDL_GL_GetProcAddress("glCreateShader"); if (_xuo_glCreateShader == NULL) { XUO_GL_MISSING(glCreateShader); }
    _xuo_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)SDL_GL_GetProcAddress("glDeleteBuffers"); if (_xuo_glDeleteBuffers == NULL) { XUO_GL_MISSING(glDeleteBuffers); }
    _xuo_glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)SDL_GL_GetProcAddress("glDeleteFramebuffers"); if (_xuo_glDeleteFramebuffers == NULL) { XUO_GL_MISSING(glDeleteFramebuffers); }
    _xuo_glDeleteProgram = (PFNGLDELETEPROGRAMPROC)SDL_GL_GetProcAddress("glDeleteProgram"); if (_xuo_glDeleteProgram == NULL) { XUO_GL_MISSING(glDeleteProgram); }
    _xuo_glDeleteShader = (PFNGLDELETESHADERPROC)SDL_GL_GetProcAddress("glDeleteShader"); if (_xuo_glDeleteShader == NULL) { XUO_GL_MISSING(glDeleteShader); }
    _xuo_glDeleteTextures = (PFNGLDELETETEXTURESPROC)SDL_GL_GetProcAddress("glDeleteTextures"); if (_xuo_glDeleteTextures == NULL) { XUO_GL_MISSING(glDeleteTextures); }
    _xuo_glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)SDL_GL_GetProcAddress("glDeleteVertexArrays"); if (_xuo_glDeleteVertexArrays == NULL) { XUO_GL_MISSING(glDeleteVertexArrays); }
    _xuo_glDepthFunc = (PFNGLDEPTHFUNCPROC)SDL_GL_GetProcAddress("glDepthFunc"); if (_xuo_glDepthFunc == NULL) { XUO_GL_MISSING(glDepthFunc); }
    _xuo_glDisable = (PFNGLDISABLEPROC)SDL_GL_GetProcAddress("glDisable"); if (_xuo_glDisable == NULL) { XUO_GL_MISSING(glDisable); }
    _xuo_glEnable = (PFNGLENABLEPROC)SDL_GL_GetProcAddress("glEnable"); if (_xuo_glEnable == NULL) { XUO_GL_MISSING(glEnable); }
    _xuo_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)SDL_GL_GetProcAddress("glEnableVertexAttribArray"); if (_xuo_glEnableVertexAttribArray == NULL) { XUO_GL_MISSING(glEnableVertexAttribArray); }
    _xuo_glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)SDL_GL_GetProcAddress("glFramebufferTexture2D"); if (_xuo_glFramebufferTexture2D == NULL) { XUO_GL_MISSING(glFramebufferTexture2D); }
    _xuo_glFramebufferTextureLayer = (PFNGLFRAMEBUFFERTEXTURELAYERPROC)SDL_GL_GetProcAddress("glFramebufferTextureLayer"); if (_xuo_glFramebufferTextureLayer == NULL) { XUO_GL_MISSING(glFramebufferTextureLayer); }
    _xuo_glGenBuffers = (PFNGLGENBUFFERSPROC)SDL_GL_GetProcAddress("glGenBuffers"); if (_xuo_glGenBuffers == NULL) { XUO_GL_MISSING(glGenBuffers); }
    _xuo_glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)SDL_GL_GetProcAddress("glGenFramebuffers"); if (_xuo_glGenFramebuffers == NULL) { XUO_GL_MISSING(glGenFramebuffers); }
    _xuo_glGenTextures = (PFNGLGENTEXTURESPROC)SDL_GL_GetProcAddress("glGenTextures"); if (_xuo_glGenTextures == NULL) { XUO_GL_MISSING(glGenTextures); }
    _xuo_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)SDL_GL_GetProcAddress("glGenVertexArrays"); if (_xuo_glGenVertexArrays == NULL) { XUO_GL_MISSING(glGenVertexArrays); }
    _xuo_glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC)SDL_GL_GetProcAddress("glGenerateMipmap"); if (_xuo_glGenerateMipmap == NULL) { XUO_GL_MISSING(glGenerateMipmap); }
    _xuo_glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)SDL_GL_GetProcAddress("glGetAttribLocation"); if (_xuo_glGetAttribLocation == NULL) { XUO_GL_MISSING(glGetAttribLocation); }
    _xuo_glGetBooleanv = (PFNGLGETBOOLEANVPROC)SDL_GL_GetProcAddress("glGetBooleanv"); if (_xuo_glGetBooleanv == NULL) { XUO_GL_MISSING(glGetBooleanv); }
    _xuo_glGetError = (PFNGLGETERRORPROC)SDL_GL_GetProcAddress("glGetError"); if (_xuo_glGetError == NULL) { XUO_GL_MISSING(glGetError); }
    _xuo_glGetFloatv = (PFNGLGETFLOATVPROC)SDL_GL_GetProcAddress("glGetFloatv"); if (_xuo_glGetFloatv == NULL) { XUO_GL_MISSING(glGetFloatv); }
    _xuo_glGetIntegerv = (PFNGLGETINTEGERVPROC)SDL_GL_GetProcAddress("glGetIntegerv"); if (_xuo_glGetIntegerv == NULL) { XUO_GL_MISSING(glGetIntegerv); }
    _xuo_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)SDL_GL_GetProcAddress("glGetProgramInfoLog"); if (_xuo_glGetProgramInfoLog == NULL) { XUO_GL_MISSING(glGetProgramInfoLog); }
    _xuo_glGetProgramiv = (PFNGLGETPROGRAMIVPROC)SDL_GL_GetProcAddress("glGetProgramiv"); if (_xuo_glGetProgramiv == NULL) { XUO_GL_MISSING(glGetProgramiv); }
    _xuo_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)SDL_GL_GetProcAddress("glGetShaderInfoLog"); if (_xuo_glGetShaderInfoLog == NULL) { XUO_GL_MISSING(glGetShaderInfoLog); }
    _xuo_glGetShaderiv = (PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress("glGetShaderiv"); if (_xuo_glGetShaderiv == NULL) { XUO_GL_MISSING(glGetShaderiv); }
    _xuo_glGetString = (PFNGLGETSTRINGPROC)SDL_GL_GetProcAddress("glGetString"); if (_xuo_glGetString == NULL) { XUO_GL_MISSING(glGetString); }
    _xuo_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)SDL_GL_GetProcAddress("glGetUniformLocation"); if (_xuo_glGetUniformLocation == NULL) { XUO_GL_MISSING(glGetUniformLocation); }
    _xuo_glIsTexture = (PFNGLISTEXTUREPROC)SDL_GL_GetProcAddress("glIsTexture"); if (_xuo_glIsTexture == NULL) { XUO_GL_MISSING(glIsTexture); }
    _xuo_glIsEnabled = (PFNGLISENABLEDPROC)SDL_GL_GetProcAddress("glIsEnabled"); if (_xuo_glIsEnabled == NULL) { XUO_GL_MISSING(glIsEnabled); }
    _xuo_glLinkProgram = (PFNGLLINKPROGRAMPROC)SDL_GL_GetProcAddress("glLinkProgram"); if (_xuo_glLinkProgram == NULL) { XUO_GL_MISSING(glLinkProgram); }
    _xuo_glPixelStorei = (PFNGLPIXELSTOREIPROC)SDL_GL_GetProcAddress("glPixelStorei"); if (_xuo_glPixelStorei == NULL) { XUO_GL_MISSING(glPixelStorei); }
    _xuo_glReadPixels = (PFNGLREADPIXELSPROC)SDL_GL_GetProcAddress("glReadPixels"); if (_xuo_glReadPixels == NULL) { XUO_GL_MISSING(glReadPixels); }
    _xuo_glScissor = (PFNGLSCISSORPROC)SDL_GL_GetProcAddress("glScissor"); if (_xuo_glScissor == NULL) { XUO_GL_MISSING(glScissor); }
    _xuo_glShaderSource = (PFNGLSHADERSOURCEPROC)SDL_GL_GetProcAddress("glShaderSource"); if (_xuo_glShaderSource == NULL) { XUO_GL_MISSING(glShaderSource); }
    _xuo_glStencilFunc = (PFNGLSTENCILFUNCPROC)SDL_GL_GetProcAddress("glStencilFunc"); if (_xuo_glStencilFunc == NULL) { XUO_GL_MISSING(glStencilFunc); }
    _xuo_glStencilMask = (PFNGLSTENCILMASKPROC)SDL_GL_GetProcAddress("glStencilMask"); if (_xuo_glStencilMask == NULL) { XUO_GL_MISSING(glStencilMask); }
    _xuo_glStencilOp = (PFNGLSTENCILOPPROC)SDL_GL_GetProcAddress("glStencilOp"); if (_xuo_glStencilOp == NULL) { XUO_GL_MISSING(glStencilOp); }
    _xuo_glTexImage2D = (PFNGLTEXIMAGE2DPROC)SDL_GL_GetProcAddress("glTexImage2D"); if (_xuo_glTexImage2D == NULL) { XUO_GL_MISSING(glTexImage2D); }
    _xuo_glTexParameteri = (PFNGLTEXPARAMETERIPROC)SDL_GL_GetProcAddress("glTexParameteri"); if (_xuo_glTexParameteri == NULL) { XUO_GL_MISSING(glTexParameteri); }
    _xuo_glUniform1f = (PFNGLUNIFORM1FPROC)SDL_GL_GetProcAddress("glUniform1f"); if (_xuo_glUniform1f == NULL) { XUO_GL_MISSING(glUniform1f); }
    _xuo_glUniform1fv = (PFNGLUNIFORM1FVPROC)SDL_GL_GetProcAddress("glUniform1fv"); if (_xuo_glUniform1fv == NULL) { XUO_GL_MISSING(glUniform1fv); }
    _xuo_glUniform1i = (PFNGLUNIFORM1IPROC)SDL_GL_GetProcAddress("glUniform1i"); if (_xuo_glUniform1i == NULL) { XUO_GL_MISSING(glUniform1i); }
    _xuo_glUniform2f = (PFNGLUNIFORM2FPROC)SDL_GL_GetProcAddress("glUniform2f"); if (_xuo_glUniform2f == NULL) { XUO_GL_MISSING(glUniform2f); }
    _xuo_glUniform3f = (PFNGLUNIFORM3FPROC)SDL_GL_GetProcAddress("glUniform3f"); if (_xuo_glUniform3f == NULL) { XUO_GL_MISSING(glUniform3f); }
    _xuo_glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)SDL_GL_GetProcAddress("glUniformMatrix4fv"); if (_xuo_glUniformMatrix4fv == NULL) { XUO_GL_MISSING(glUniformMatrix4fv); }
    _xuo_glUseProgram = (PFNGLUSEPROGRAMPROC)SDL_GL_GetProcAddress("glUseProgram"); if (_xuo_glUseProgram == NULL) { XUO_GL_MISSING(glUseProgram); }
    _xuo_glValidateProgram = (PFNGLVALIDATEPROGRAMPROC)SDL_GL_GetProcAddress("glValidateProgram"); if (_xuo_glValidateProgram == NULL) { XUO_GL_MISSING(glValidateProgram); }
    _xuo_glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)SDL_GL_GetProcAddress("glVertexAttribPointer"); if (_xuo_glVertexAttribPointer == NULL) { XUO_GL_MISSING(glVertexAttribPointer); }
    _xuo_glViewport = (PFNGLVIEWPORTPROC)SDL_GL_GetProcAddress("glViewport"); if (_xuo_glViewport == NULL) { XUO_GL_MISSING(glViewport); }
    _xuo_glDrawArrays = (PFNGLDRAWARRAYSPROC)SDL_GL_GetProcAddress("glDrawArrays"); if (_xuo_glDrawArrays == NULL) { XUO_GL_MISSING(glDrawArrays); }
    _xuo_glDrawElements = (PFNGLDRAWELEMENTSPROC)SDL_GL_GetProcAddress("glDrawElements"); if (_xuo_glDrawElements == NULL) { XUO_GL_MISSING(glDrawElements); }
    _xuo_glTexImage3D = (PFNGLTEXIMAGE3DPROC)SDL_GL_GetProcAddress("glTexImage3D"); if (_xuo_glTexImage3D == NULL) { XUO_GL_MISSING(glTexImage3D); }
    _xuo_glCopyImageSubData = (PFNGLCOPYIMAGESUBDATAPROC)SDL_GL_GetProcAddress("glCopyImageSubData"); if (_xuo_glCopyImageSubData == NULL) { XUO_GL_MISSING(glCopyImageSubData); }
    _xuo_glCopyTexSubImage2D = (PFNGLCOPYTEXSUBIMAGE2DPROC)SDL_GL_GetProcAddress("glCopyTexSubImage2D"); if (_xuo_glCopyTexSubImage2D == NULL) { XUO_GL_MISSING(glCopyTexSubImage2D); }
    _xuo_glCopyTexSubImage3D = (PFNGLCOPYTEXSUBIMAGE3DPROC)SDL_GL_GetProcAddress("glCopyTexSubImage3D"); if (_xuo_glCopyTexSubImage3D == NULL) { XUO_GL_MISSING(glCopyTexSubImage3D); }
    _xuo_glPushDebugGroup = (PFNGLPUSHDEBUGGROUPPROC)SDL_GL_GetProcAddress("glPushDebugGroup"); if (_xuo_glPushDebugGroup == NULL) { XUO_GL_MISSING(glPushDebugGroup); }
    _xuo_glPopDebugGroup = (PFNGLPOPDEBUGGROUPPROC)SDL_GL_GetProcAddress("glPopDebugGroup"); if (_xuo_glPopDebugGroup == NULL) { XUO_GL_MISSING(glPopDebugGroup); }
    _xuo_glDebugMessageCallback = (PFNGLDEBUGMESSAGECALLBACKPROC)SDL_GL_GetProcAddress("glDebugMessageCallback"); if (_xuo_glDebugMessageCallback == NULL) { XUO_GL_MISSING(glDebugMessageCallback); }
    _xuo_glDebugMessageControl = (PFNGLDEBUGMESSAGECONTROLPROC)SDL_GL_GetProcAddress("glDebugMessageControl"); if (_xuo_glDebugMessageControl == NULL) { XUO_GL_MISSING(glDebugMessageControl); }
}

#ifdef glActiveTexture
#undef glActiveTexture
#endif
#define glActiveTexture (_xuo_glActiveTexture)
#ifdef glAttachShader
#undef glAttachShader
#endif
#define glAttachShader (_xuo_glAttachShader)
#ifdef glBindBuffer
#undef glBindBuffer
#endif
#define glBindBuffer (_xuo_glBindBuffer)
#ifdef glBindFramebuffer
#undef glBindFramebuffer
#endif
#define glBindFramebuffer (_xuo_glBindFramebuffer)
#ifdef glBindTexture
#undef glBindTexture
#endif
#define glBindTexture (_xuo_glBindTexture)
#ifdef glBindVertexArray
#undef glBindVertexArray
#endif
#define glBindVertexArray (_xuo_glBindVertexArray)
#ifdef glBlendEquation
#undef glBlendEquation
#endif
#define glBlendEquation (_xuo_glBlendEquation)
#ifdef glBlendFunc
#undef glBlendFunc
#endif
#define glBlendFunc (_xuo_glBlendFunc)
#ifdef glBufferData
#undef glBufferData
#endif
#define glBufferData (_xuo_glBufferData)
#ifdef glBufferSubData
#undef glBufferSubData
#endif
#define glBufferSubData (_xuo_glBufferSubData)
#ifdef glCheckFramebufferStatus
#undef glCheckFramebufferStatus
#endif
#define glCheckFramebufferStatus (_xuo_glCheckFramebufferStatus)
#ifdef glClear
#undef glClear
#endif
#define glClear (_xuo_glClear)
#ifdef glClearColor
#undef glClearColor
#endif
#define glClearColor (_xuo_glClearColor)
#ifdef glClearDepthf
#undef glClearDepthf
#endif
#define glClearDepthf (_xuo_glClearDepthf)
#ifdef glClearStencil
#undef glClearStencil
#endif
#define glClearStencil (_xuo_glClearStencil)
#ifdef glColorMask
#undef glColorMask
#endif
#define glColorMask (_xuo_glColorMask)
#ifdef glCompileShader
#undef glCompileShader
#endif
#define glCompileShader (_xuo_glCompileShader)
#ifdef glCreateProgram
#undef glCreateProgram
#endif
#define glCreateProgram (_xuo_glCreateProgram)
#ifdef glCreateShader
#undef glCreateShader
#endif
#define glCreateShader (_xuo_glCreateShader)
#ifdef glDeleteBuffers
#undef glDeleteBuffers
#endif
#define glDeleteBuffers (_xuo_glDeleteBuffers)
#ifdef glDeleteFramebuffers
#undef glDeleteFramebuffers
#endif
#define glDeleteFramebuffers (_xuo_glDeleteFramebuffers)
#ifdef glDeleteProgram
#undef glDeleteProgram
#endif
#define glDeleteProgram (_xuo_glDeleteProgram)
#ifdef glDeleteShader
#undef glDeleteShader
#endif
#define glDeleteShader (_xuo_glDeleteShader)
#ifdef glDeleteTextures
#undef glDeleteTextures
#endif
#define glDeleteTextures (_xuo_glDeleteTextures)
#ifdef glDeleteVertexArrays
#undef glDeleteVertexArrays
#endif
#define glDeleteVertexArrays (_xuo_glDeleteVertexArrays)
#ifdef glDepthFunc
#undef glDepthFunc
#endif
#define glDepthFunc (_xuo_glDepthFunc)
#ifdef glDisable
#undef glDisable
#endif
#define glDisable (_xuo_glDisable)
#ifdef glEnable
#undef glEnable
#endif
#define glEnable (_xuo_glEnable)
#ifdef glEnableVertexAttribArray
#undef glEnableVertexAttribArray
#endif
#define glEnableVertexAttribArray (_xuo_glEnableVertexAttribArray)
#ifdef glFramebufferTexture2D
#undef glFramebufferTexture2D
#endif
#define glFramebufferTexture2D (_xuo_glFramebufferTexture2D)
#ifdef glFramebufferTextureLayer
#undef glFramebufferTextureLayer
#endif
#define glFramebufferTextureLayer (_xuo_glFramebufferTextureLayer)
#ifdef glGenBuffers
#undef glGenBuffers
#endif
#define glGenBuffers (_xuo_glGenBuffers)
#ifdef glGenFramebuffers
#undef glGenFramebuffers
#endif
#define glGenFramebuffers (_xuo_glGenFramebuffers)
#ifdef glGenTextures
#undef glGenTextures
#endif
#define glGenTextures (_xuo_glGenTextures)
#ifdef glGenVertexArrays
#undef glGenVertexArrays
#endif
#define glGenVertexArrays (_xuo_glGenVertexArrays)
#ifdef glGenerateMipmap
#undef glGenerateMipmap
#endif
#define glGenerateMipmap (_xuo_glGenerateMipmap)
#ifdef glGetAttribLocation
#undef glGetAttribLocation
#endif
#define glGetAttribLocation (_xuo_glGetAttribLocation)
#ifdef glGetBooleanv
#undef glGetBooleanv
#endif
#define glGetBooleanv (_xuo_glGetBooleanv)
#ifdef glGetError
#undef glGetError
#endif
#define glGetError (_xuo_glGetError)
#ifdef glGetFloatv
#undef glGetFloatv
#endif
#define glGetFloatv (_xuo_glGetFloatv)
#ifdef glGetIntegerv
#undef glGetIntegerv
#endif
#define glGetIntegerv (_xuo_glGetIntegerv)
#ifdef glGetProgramInfoLog
#undef glGetProgramInfoLog
#endif
#define glGetProgramInfoLog (_xuo_glGetProgramInfoLog)
#ifdef glGetProgramiv
#undef glGetProgramiv
#endif
#define glGetProgramiv (_xuo_glGetProgramiv)
#ifdef glGetShaderInfoLog
#undef glGetShaderInfoLog
#endif
#define glGetShaderInfoLog (_xuo_glGetShaderInfoLog)
#ifdef glGetShaderiv
#undef glGetShaderiv
#endif
#define glGetShaderiv (_xuo_glGetShaderiv)
#ifdef glGetString
#undef glGetString
#endif
#define glGetString (_xuo_glGetString)
#ifdef glGetUniformLocation
#undef glGetUniformLocation
#endif
#define glGetUniformLocation (_xuo_glGetUniformLocation)
#ifdef glIsTexture
#undef glIsTexture
#endif
#define glIsTexture (_xuo_glIsTexture)
#ifdef glIsEnabled
#undef glIsEnabled
#endif
#define glIsEnabled (_xuo_glIsEnabled)
#ifdef glLinkProgram
#undef glLinkProgram
#endif
#define glLinkProgram (_xuo_glLinkProgram)
#ifdef glPixelStorei
#undef glPixelStorei
#endif
#define glPixelStorei (_xuo_glPixelStorei)
#ifdef glReadPixels
#undef glReadPixels
#endif
#define glReadPixels (_xuo_glReadPixels)
#ifdef glScissor
#undef glScissor
#endif
#define glScissor (_xuo_glScissor)
#ifdef glShaderSource
#undef glShaderSource
#endif
#define glShaderSource (_xuo_glShaderSource)
#ifdef glStencilFunc
#undef glStencilFunc
#endif
#define glStencilFunc (_xuo_glStencilFunc)
#ifdef glStencilMask
#undef glStencilMask
#endif
#define glStencilMask (_xuo_glStencilMask)
#ifdef glStencilOp
#undef glStencilOp
#endif
#define glStencilOp (_xuo_glStencilOp)
#ifdef glTexImage2D
#undef glTexImage2D
#endif
#define glTexImage2D (_xuo_glTexImage2D)
#ifdef glTexParameteri
#undef glTexParameteri
#endif
#define glTexParameteri (_xuo_glTexParameteri)
#ifdef glUniform1f
#undef glUniform1f
#endif
#define glUniform1f (_xuo_glUniform1f)
#ifdef glUniform1fv
#undef glUniform1fv
#endif
#define glUniform1fv (_xuo_glUniform1fv)
#ifdef glUniform1i
#undef glUniform1i
#endif
#define glUniform1i (_xuo_glUniform1i)
#ifdef glUniform2f
#undef glUniform2f
#endif
#define glUniform2f (_xuo_glUniform2f)
#ifdef glUniform3f
#undef glUniform3f
#endif
#define glUniform3f (_xuo_glUniform3f)
#ifdef glUniformMatrix4fv
#undef glUniformMatrix4fv
#endif
#define glUniformMatrix4fv (_xuo_glUniformMatrix4fv)
#ifdef glUseProgram
#undef glUseProgram
#endif
#define glUseProgram (_xuo_glUseProgram)
#ifdef glValidateProgram
#undef glValidateProgram
#endif
#define glValidateProgram (_xuo_glValidateProgram)
#ifdef glVertexAttribPointer
#undef glVertexAttribPointer
#endif
#define glVertexAttribPointer (_xuo_glVertexAttribPointer)
#ifdef glViewport
#undef glViewport
#endif
#define glViewport (_xuo_glViewport)
#ifdef glDrawArrays
#undef glDrawArrays
#endif
#define glDrawArrays (_xuo_glDrawArrays)
#ifdef glDrawElements
#undef glDrawElements
#endif
#define glDrawElements (_xuo_glDrawElements)
#ifdef glTexImage3D
#undef glTexImage3D
#endif
#define glTexImage3D (_xuo_glTexImage3D)
#ifdef glCopyImageSubData
#undef glCopyImageSubData
#endif
#define glCopyImageSubData (_xuo_glCopyImageSubData)
#ifdef glCopyTexSubImage2D
#undef glCopyTexSubImage2D
#endif
#define glCopyTexSubImage2D (_xuo_glCopyTexSubImage2D)
#ifdef glCopyTexSubImage3D
#undef glCopyTexSubImage3D
#endif
#define glCopyTexSubImage3D (_xuo_glCopyTexSubImage3D)
#ifdef glPushDebugGroup
#undef glPushDebugGroup
#endif
#define glPushDebugGroup (_xuo_glPushDebugGroup)
#ifdef glPopDebugGroup
#undef glPopDebugGroup
#endif
#define glPopDebugGroup (_xuo_glPopDebugGroup)
#ifdef glDebugMessageCallback
#undef glDebugMessageCallback
#endif
#define glDebugMessageCallback (_xuo_glDebugMessageCallback)
#ifdef glDebugMessageControl
#undef glDebugMessageControl
#endif
#define glDebugMessageControl (_xuo_glDebugMessageControl)

static inline int gfx_gl_load_proc_address(void)
{
    _xuo_missing_count = 0;
    _xuo_missing_first = NULL;
    _xuo_missing_names[0] = 0;
    gfx_gl_load_proc_address_into();
    return _xuo_missing_count;
}

// Human-readable summary of the last load; NULL when nothing was missing.
static inline const char *gfx_gl_missing_summary(void)
{
    return _xuo_missing_count == 0 ? NULL : _xuo_missing_names;
}
#endif // USE_GLES || XUO_GLES_DESKTOP || XUO_GL_RUNTIME

#endif // #ifndef EXTERNAL_GFX_H

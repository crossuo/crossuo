// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Danny Angelo Carminati Grein

#include "glctx.h"
#include "gfx.h"

#include <stdio.h>
#include <string.h>

#if defined(XUO_EMSC)
    #include <emscripten/html5.h>
#endif

// One process, one context: the flavour is decided once, when the renderer
// takes its context, and every later query reads the cached answer.
static enum glctx_flavor s_flavor = GLCTX_FLAVOR_NONE;
static glctx_caps s_caps;
static bool s_probed = false;

// Probe results are cached separately from the flavour a build hard-codes:
// a forced build (RENDER_GL3/GLES2) never probes at all.
static bool s_probe_attempted = false;

static const glctx_shader_dialect s_dialect_none = {
    "330", "layout (location = %d", "in ", "out ", ""
};

// GL3: the shader dialect the desktop build always used
static const glctx_shader_dialect s_dialect_gl3 = {
    "330", // overridden to "150" on Apple by glctx_shader_dialect_get()
    "layout (location = %d",
    "in ",
    "out ",
    "",
};

// GLES3: same shaders, ES dialect; the array sampler needs an explicit
// precision and the old texture2D() spelling maps onto texture()
static const glctx_shader_dialect s_dialect_gles3 = {
    "300 es",
    "layout (location = %d",
    "in ",
    "out ",
    "precision highp sampler2DArray;\n"
    "#define texture2D texture\n",
};

static const glctx_shader_dialect s_dialect_gles2 = {
    "100",
    "attribute ",
    "varying ",
    "varying ",
    "",
};

const char *glctx_flavor_name(enum glctx_flavor flavor)
{
    switch (flavor)
    {
        case GLCTX_FLAVOR_GL3:
            return "gl3";
        case GLCTX_FLAVOR_GLES3:
            return "gles3";
        default:
            return "none";
    }
}

enum glctx_flavor glctx_flavor_from_name(const char *name)
{
    if (name == NULL || name[0] == '\0' || strcmp(name, "auto") == 0)
    {
        return GLCTX_FLAVOR_NONE;
    }
    if (strcmp(name, "gl3") == 0 || strcmp(name, "gl") == 0 || strcmp(name, "desktop") == 0)
    {
        return GLCTX_FLAVOR_GL3;
    }
    if (strcmp(name, "gles3") == 0 || strcmp(name, "gles") == 0 || strcmp(name, "es3") == 0)
    {
        return GLCTX_FLAVOR_GLES3;
    }
    return GLCTX_FLAVOR_NONE;
}

enum glctx_flavor glctx_flavor_get()
{
    return s_flavor;
}

const glctx_caps *glctx_caps_get()
{
    return &s_caps;
}

void glctx_apply_attributes(enum glctx_flavor flavor, bool debug_context)
{
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    if (flavor == GLCTX_FLAVOR_GLES3)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#if !defined(XUO_EMSC)
        // ES never comes from GLX: the EGL driver has to be selected before
        // SDL's video subsystem starts, and that selection is sticky
        SDL_SetHint(SDL_HINT_VIDEO_FORCE_EGL, "1");
#endif
        // EGL has no debug-context flag; KHR_debug is opted into after
        // creation when the driver advertises it
        (void)debug_context;
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        return;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#if defined(__APPLE__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#else
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_FLAGS, debug_context ? SDL_GL_CONTEXT_DEBUG_FLAG : 0);
#endif
}

// Reads the version/extension facts of the context that is current right now.
static void glctx_fill_caps(enum glctx_flavor flavor)
{
    memset(&s_caps, 0, sizeof(s_caps));
    s_caps.is_es = (flavor == GLCTX_FLAVOR_GLES3);

    const char *version = (const char *)glGetString(GL_VERSION);
    const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
    // glGetString(GL_EXTENSIONS) is illegal on core profiles (and ES3): the
    // driver returns NULL and raises a sticky GL_INVALID_ENUM that would
    // otherwise surface at the first GL_CHECK afterwards. Consume it.
    while (glGetError() != GL_NO_ERROR)
    {
    }
    if (extensions == NULL)
    {
        // core profiles and every ES3 context use the indexed API; glGetString
        // returns NULL there and the renderer never needed the list before
        extensions = "";
    }

    s_caps.version_major = 0;
    s_caps.version_minor = 0;
    if (version != NULL)
    {
        const char *p = version;
        while (*p != '\0' && (*p < '0' || *p > '9'))
        {
            ++p; // "OpenGL ES 3.2 ..." / "4.6 (Core Profile) ..."
        }
        if (*p >= '0' && *p <= '9')
        {
            s_caps.version_major = *p - '0';
            if (p[1] == '.')
            {
                s_caps.version_minor = p[2] - '0';
            }
        }
    }

    // ES: exact core-version facts; desktop: probe the extensions that carry
    // the same functionality (all core since 4.3 / 3.0 respectively)
    if (s_caps.is_es)
    {
        s_caps.has_vao = s_caps.version_major >= 3; // core in ES3, absent in ES2
        s_caps.has_sampler_array = true;  // core in ES3
        s_caps.has_copy_subimage = true;  // core in ES3.2, emulated below on 3.0
#if !defined(XUO_EMSC)
        s_caps.has_debug_output = strstr(extensions, "GL_KHR_debug") != NULL ||
                                  glDebugMessageCallback != NULL;
#endif
        if (s_caps.version_major == 3 && s_caps.version_minor < 2)
        {
            // glCopyImageSubData landed in ES 3.2; Render_ArtArrayPlace has an
            // emulation that needs only core ES3
            s_caps.has_copy_subimage = true;
        }
    }
    else
    {
        s_caps.has_vao = true; // core in GL3.0
        s_caps.has_sampler_array = true;
        s_caps.has_copy_subimage =
            (s_caps.version_major > 4 ||
             (s_caps.version_major == 4 && s_caps.version_minor >= 3)) ||
            strstr(extensions, "GL_ARB_copy_image") != NULL;
        s_caps.has_debug_output = (s_caps.version_major >= 4 && s_caps.version_minor >= 3) ||
                                  strstr(extensions, "GL_KHR_debug") != NULL;
    }
}

const glctx_shader_dialect *glctx_shader_dialect_get()
{
    switch (s_flavor)
    {
        case GLCTX_FLAVOR_GL3:
            return &s_dialect_gl3;
        case GLCTX_FLAVOR_GLES3:
            return &s_dialect_gles3;
        default:
            break;
    }
#if defined(USE_GLES2)
    // Every ES build - WebGL, the dedicated desktop one and the automatic
    // binary before it has probed - speaks GLSL ES. The GLES2/WebGL build
    // asks the context for 1.00, the ES3 builds for 3.00.
  #if defined(XUO_EMSC) || defined(XUO_GLES_DESKTOP) || defined(XUO_GL_RUNTIME)
    return &s_dialect_gles3;
  #else
    return &s_dialect_gles2;
  #endif
#else
    return &s_dialect_none;
#endif
}

const char *glctx_shader_version_string()
{
    static char s_version[64];
    if (s_version[0] == '\0')
    {
        snprintf(s_version, sizeof(s_version), "#version %s", glctx_shader_dialect_get()->version);
    }
    return s_version;
}

// Creates and destroys one context to find out whether `flavor` is available.
// Must run before any SDL_WINDOW_OPENGL window exists: SDL picks the GL/EGL
// library the first time it sees one and never revisits that choice.
static bool glctx_try_create(enum glctx_flavor flavor, bool debug_context)
{
    glctx_apply_attributes(flavor, debug_context);

    SDL_Window *window = SDL_CreateWindow(
        "crossuo-gl-probe", 1, 1, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (window == NULL)
    {
        fprintf(stderr, "glctx: %s unavailable (window: %s), trying the next one\n",
                glctx_flavor_name(flavor), SDL_GetError());
        return false;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (context == NULL)
    {
        fprintf(stderr, "glctx: %s unavailable (%s), trying the next one\n",
                glctx_flavor_name(flavor), SDL_GetError());
        SDL_DestroyWindow(window);
        return false;
    }

    SDL_GL_MakeCurrent(window, context);
#if defined(USE_GLES) || defined(XUO_GLES_DESKTOP) || defined(XUO_GL_RUNTIME)
    // the entry-point table is empty until it is resolved against a current
    // context; every capability query below goes through it. Desktop-GL
    // builds resolve through gl3w/glew instead and have no such table.
    gfx_gl_load_proc_address();
#endif
    glctx_fill_caps(flavor);
    SDL_GL_MakeCurrent(window, NULL);
    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);

    s_flavor = flavor;
    return true;
}

void glctx_mark_flavor(enum glctx_flavor flavor)
{
    s_flavor = flavor;
    s_probed = true;
    memset(&s_caps, 0, sizeof(s_caps)); // pending: glctx_refresh_caps completes
}

void glctx_refresh_caps(void)
{
    glctx_fill_caps(s_flavor);
}

enum glctx_flavor glctx_probe(enum glctx_flavor preferred)
{
    if (s_probed)
    {
        return s_flavor;
    }
    s_probed = true;

#if defined(XUO_EMSC)
    // The browser owns the WebGL2 context the bootstrap created on the
    // canvas: there is nothing to choose, and probing would create a second
    // context the page did not ask for.
    s_flavor = GLCTX_FLAVOR_GLES3;
    glctx_fill_caps(s_flavor);
    return s_flavor;
#elif defined(USE_GLES2) && defined(XUO_GLES_DESKTOP) && !defined(XUO_GL_RUNTIME)
    // The ES-only build asks for one thing; keep its behaviour exactly as it
    // was for the release binaries that already ship that way. The automatic
    // binary defines XUO_GLES_DESKTOP too (it uses the same header set), so it
    // must fall through to the real cascade below.
    s_flavor = GLCTX_FLAVOR_GLES3;
    return s_flavor;
#else
    if (preferred == GLCTX_FLAVOR_GLES3)
    {
        if (!glctx_try_create(GLCTX_FLAVOR_GLES3, false))
        {
            fprintf(
                stderr,
                "glctx: OpenGL ES 3 was requested but the driver could not provide it\n");
            s_flavor = GLCTX_FLAVOR_NONE;
        }
        return s_flavor;
    }
    if (preferred == GLCTX_FLAVOR_GL3)
    {
        if (!glctx_try_create(GLCTX_FLAVOR_GL3, false))
        {
            fprintf(stderr, "glctx: OpenGL 3.2 core was requested but is unavailable\n");
            s_flavor = GLCTX_FLAVOR_NONE;
        }
        return s_flavor;
    }

    // cascade: desktop first, ES second. A failure here is normal (ES-only
    // stacks), so keep the SDL error quiet until every candidate failed.
    if (glctx_try_create(GLCTX_FLAVOR_GL3, false))
    {
        return s_flavor;
    }
    if (glctx_try_create(GLCTX_FLAVOR_GLES3, false))
    {
        return s_flavor;
    }

    fprintf(
        stderr,
        "glctx: no usable OpenGL context (tried desktop 3.2 core, then OpenGL ES 3): %s\n",
        SDL_GetError());
    s_flavor = GLCTX_FLAVOR_NONE;
    return s_flavor;
#endif
}

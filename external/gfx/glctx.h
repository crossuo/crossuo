// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Danny Angelo Carminati Grein

#pragma once

// Runtime GL flavour: which context the client actually got, and the shader
// dialect that goes with it.
//
// The renderer used to be chosen at build time (RENDER_GL3 / RENDER_GLES2),
// which forced users on an ES-only stack (an ARM SoC running only
// libGLESv2/EGL) to install a second binary. Everything that differed is
// collected here and queried at run time instead: one binary asks for a
// desktop GL 3.2 core context, falls back to GLES 3.0 when the driver cannot
// provide it, and picks the matching shader source.
//
// The GLES2/WebGL and GL1 builds keep their build-time behaviour; they set
// the flavour once at startup and never probe.

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum glctx_flavor
{
    // nothing chosen yet: the first glctx_probe() decides
    GLCTX_FLAVOR_NONE = 0,
    // desktop OpenGL 3.2+ core profile, GLSL 3.30
    GLCTX_FLAVOR_GL3,
    // OpenGL ES 3.0+, GLSL ES 3.00 (angle brackets kept out of comments)
    GLCTX_FLAVOR_GLES3,
};

// What the chosen context can do. Feature queries here replace the old
// `#if defined(USE_GL3) || defined(USE_GLES)` gates in the renderer.
typedef struct glctx_caps_
{
    bool is_es;              // ES dialect: texel_fetch is `texelFetch`, etc.
    int version_major;
    int version_minor;
    bool has_vao;            // vertex array objects
    bool has_sampler_array;  // sampler2DArray + glTexImage3D
    bool has_copy_subimage;  // glCopyImageSubData (the art atlas grow path)
    bool has_debug_output;   // KHR_debug
} glctx_caps;

// Resolves the flavour once and caches it. `preferred` may be GLCTX_FLAVOR_NONE
// (cascade: GL3 then GLES3) or one of the concrete values (force from
// XUO_RENDER / the config), in which case that is the only candidate.
// Returns the chosen flavour; GLCTX_FLAVOR_NONE when nothing could be created.
enum glctx_flavor glctx_probe(enum glctx_flavor preferred);

// The flavour chosen by glctx_probe() (GLCTX_FLAVOR_NONE before it runs).
enum glctx_flavor glctx_flavor_get();

// Capabilities of the chosen context; safe to call before glctx_probe() (all
// false) and stable afterwards.
const glctx_caps *glctx_caps_get();

// Declares the flavour for builds that force one at compile time (they never
// probe): sets the flavour and marks the caps as pending. glctx_refresh_caps
// completes them once the context is current. No GL calls.
void glctx_mark_flavor(enum glctx_flavor flavor);

// (Re-)reads the capability facts of the currently current context. Forced
// builds call this from Render_Init; probing builds already have fresh caps.
void glctx_refresh_caps(void);

// Parses an XUO_RENDER-style name ("auto", "gl3", "gles3", "gles2", "gl1").
// Unknown names return GLCTX_FLAVOR_NONE so the caller can warn.
enum glctx_flavor glctx_flavor_from_name(const char *name);

// Short name for logs ("gl3", "gles3", "none").
const char *glctx_flavor_name(enum glctx_flavor flavor);

// Sets the SDL GL attributes for `flavor` without creating anything.
void glctx_apply_attributes(enum glctx_flavor flavor, bool debug_context);

// Shader dialect parameters for the current flavour.
typedef struct glctx_shader_dialect_
{
    const char *version;         // GLSL version line contents, e.g. "330"
    const char *attribute;       // per-attribute input prefix
    const char *in;              // stage input qualifier
    const char *out;             // stage output qualifier
    const char *header_extra;    // dialect preamble (precision/defines)
} glctx_shader_dialect;

const glctx_shader_dialect *glctx_shader_dialect_get();

// The complete "#version ..." line for the current flavour, for consumers that
// take a version string directly (the ImGui GL3 backend).
const char *glctx_shader_version_string();

#ifdef __cplusplus
}
#endif

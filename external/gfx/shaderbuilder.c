// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Danny Angelo Carminati Grein

#include "shaderbuilder.h"
#include "glctx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The triangle the renderer draws with is the same everywhere; what changes
// with the context flavour is the version line, the attribute/varying
// qualifiers and (on ES) the sampler precision plus the legacy texture2D()
// spelling. Assembling the source here keeps one copy of the shader logic for
// every backend instead of one frozen per build.

static const char *s_vertex = NULL;
static const char *s_fragment = NULL;

// attributes are declared in a fixed order: inPos, inUV, inColor, inNormal,
// inLayer (locations 0..4), which the vertex layout depends on
static const char *s_attributes[] = {
    "vec3 inPos;",
    "vec2 inUV;",
    "vec4 inColor;",
    "vec3 inNormal;",
    "float inLayer;",
};

static const char *VERTEX_BODY =
    "    vUV = inUV;\n"
    "    vColor = inColor;\n"
    "    vNormal = inNormal;\n"
    "    vLayer = inLayer;\n"
    "    gl_Position = uProjectionView * uModel * vec4(inPos, 1.0);\n";

static const char *FRAGMENT_BODY =
    "    vec4 textureColor = sampleTex(vUV, vLayer);\n"
    "    if (uAlphaTestEnabled != 0 && textureColor.a <= uAlphaRef) {\n"
    "        discard;\n"
    "    }\n"
    "    \n"
    "    int baseMode = drawMode;\n"
    "    float checkTrans = 0.0;\n"
    "    if (drawMode >= 100) {\n"
    "        checkTrans = 1.0;\n"
    "        baseMode = drawMode - 100;\n"
    "    }\n"
    "    if (textureColor.a != 0.0)\n"
    "    {\n"
    "        // SDM_LAND = 6 or SDM_LAND_COLORED = 7: apply lighting\n"
    "        if (baseMode == 6 || baseMode == 7)\n"
    "        {\n"
    "            vec3 lightDir = normalize(vec3(-1.0, -1.0, 0.5));\n"
    "            vec3 normal = normalize(vNormal);\n"
    "            float light = max(dot(normal, lightDir) + 0.5, 0.0);\n"
    "            \n"
    "            if (baseMode == 7) // SDM_LAND_COLORED\n"
    "            {\n"
    "                int index = int(textureColor.r * 31.875) * 3;\n"
    "                xuo_fragColor = (vec4(colors[index], colors[index + 1], colors[index + 2], textureColor.a) * vColor) * light;\n"
    "            }\n"
    "            else // SDM_LAND\n"
    "            {\n"
    "                xuo_fragColor = (textureColor * vColor) * light;\n"
    "            }\n"
    "        }\n"
    "        // SDM_COLORED = 1 || SDM_PARTIAL_HUE = 2\n"
    "        else if (baseMode == 1 || (baseMode == 2 && textureColor.r == textureColor.g && textureColor.r == textureColor.b))\n"
    "        {\n"
    "            int index = int(textureColor.r * 31.875) * 3;\n"
    "            xuo_fragColor = vec4(colors[index], colors[index + 1], colors[index + 2], textureColor.a) * vColor;\n"
    "        }\n"
    "        // SDM_TEXT_COLORED = 4 || SDM_TEXT_COLORED_NO_BLACK = 3\n"
    "        else if (baseMode == 4 || (baseMode == 3 && textureColor.r > 0.04))\n"
    "        {\n"
    "            xuo_fragColor = vec4(colors[90], colors[91], colors[92], textureColor.a) * vColor;\n"
    "        }\n"
    "        // SDM_SPECTRAL = 10, SDM_SPECIAL_SPECTRAL = 11, SDM_SHADOW = 12\n"
    "        else if (baseMode > 9)\n"
    "        {\n"
    "            float red = textureColor.r;\n"
    "            if (baseMode > 10)\n"
    "            {\n"
    "                if (baseMode > 11)\n"
    "                    red = 0.6;\n"
    "                else\n"
    "                    red *= 0.5;\n"
    "            }\n"
    "            else\n"
    "                red *= 1.5;\n"
    "            xuo_fragColor = vec4(red, red, red, textureColor.a) * vColor;\n"
    "        }\n"
    "        else\n"
    "        {\n"
    "            xuo_fragColor = textureColor * vColor;\n"
    "        }\n"
    "    }\n"
    "    else\n"
    "    {\n"
    "        xuo_fragColor = textureColor * vColor;\n"
    "    }\n"
    "    if (checkTrans > 0.0)\n"
    "    {\n"
    "        // fc = the fragment position in game window coordinates (y-down),\n"
    "        // i.e. gl_FragCoord relative to the viewport origin\n"
    "        vec2 fc = gl_FragCoord.xy - uViewport;\n"
    "        float d = distance(fc, uCircleTrans.xy);\n"
    "        if (d >= uCircleTrans.z)\n"
    "        {\n"
    "            // outside the circle of transparency: fully opaque\n"
    "            xuo_fragColor.a = 1.0;\n"
    "        }\n"
    "        else\n"
    "        {\n"
    "            // inside: alpha blended, staying fully transparent until the\n"
    "            // last FADE_START fraction of the radius, then easing strongly\n"
    "            // to opaque at the circle edge\n"
    "            const float FADE_START = 0.9;\n"
    "            float t = clamp((d / uCircleTrans.z - FADE_START) / (1.0 - FADE_START), 0.0, 1.0);\n"
    "            float fade = t * t * (3.0 - 2.0 * t);\n"
    "            xuo_fragColor.a = mix(xuo_fragColor.a, 1.0, fade);\n"
    "        }\n"
    "    if (uAlphaTestEnabled != 0 && textureColor.a <= uAlphaRef) {\n"
    "        discard;\n"
    "    }\n"
    "    \n"
    "    int baseMode = drawMode;\n"
    "    float checkTrans = 0.0;\n"
    "    if (drawMode >= 100) {\n"
    "        checkTrans = 1.0;\n"
    "        baseMode = drawMode - 100;\n"
    "    }\n"
    "    if (textureColor.a != 0.0)\n"
    "    {\n"
    "        // SDM_LAND = 6 or SDM_LAND_COLORED = 7: apply lighting\n"
    "        if (baseMode == 6 || baseMode == 7)\n"
    "        {\n"
    "            vec3 lightDir = normalize(vec3(-1.0, -1.0, 0.5));\n"
    "            vec3 normal = normalize(vNormal);\n"
    "            float light = max(dot(normal, lightDir) + 0.5, 0.0);\n"
    "            \n"
    "            if (baseMode == 7) // SDM_LAND_COLORED\n"
    "            {\n"
    "                int index = int(textureColor.r * 31.875) * 3;\n"
    "                xuo_fragColor = (vec4(colors[index], colors[index + 1], colors[index + 2], textureColor.a) * vColor) * light;\n"
    "            }\n"
    "            else // SDM_LAND\n"
    "            {\n"
    "                xuo_fragColor = (textureColor * vColor) * light;\n"
    "            }\n"
    "        }\n"
    "        // SDM_COLORED = 1 || SDM_PARTIAL_HUE = 2\n"
    "        else if (baseMode == 1 || (baseMode == 2 && textureColor.r == textureColor.g && textureColor.r == textureColor.b))\n"
    "        {\n"
    "            int index = int(textureColor.r * 31.875) * 3;\n"
    "            xuo_fragColor = vec4(colors[index], colors[index + 1], colors[index + 2], textureColor.a) * vColor;\n"
    "        }\n"
    "        // SDM_TEXT_COLORED = 4 || SDM_TEXT_COLORED_NO_BLACK = 3\n"
    "        else if (baseMode == 4 || (baseMode == 3 && textureColor.r > 0.04))\n"
    "        {\n"
    "            xuo_fragColor = vec4(colors[90], colors[91], colors[92], textureColor.a) * vColor;\n"
    "        }\n"
    "        // SDM_SPECTRAL = 10, SDM_SPECIAL_SPECTRAL = 11, SDM_SHADOW = 12\n"
    "        else if (baseMode > 9)\n"
    "        {\n"
    "            float red = textureColor.r;\n"
    "            if (baseMode > 10)\n"
    "            {\n"
    "                if (baseMode > 11)\n"
    "                    red = 0.6;\n"
    "                else\n"
    "                    red *= 0.5;\n"
    "            }\n"
    "            else\n"
    "                red *= 1.5;\n"
    "            xuo_fragColor = vec4(red, red, red, textureColor.a) * vColor;\n"
    "        }\n"
    "        else\n"
    "        {\n"
    "            xuo_fragColor = textureColor * vColor;\n"
    "        }\n"
    "    }\n"
    "    else\n"
    "    {\n"
    "        xuo_fragColor = textureColor * vColor;\n"
    "    }\n"
    "    if (checkTrans > 0.0)\n"
    "    {\n"
    "        // fc = the fragment position in game window coordinates (y-down),\n"
    "        // i.e. gl_FragCoord relative to the viewport origin\n"
    "        vec2 fc = gl_FragCoord.xy - uViewport;\n"
    "        float d = distance(fc, uCircleTrans.xy);\n"
    "        if (d >= uCircleTrans.z)\n"
    "        {\n"
    "            // outside the circle of transparency: fully opaque\n"
    "            xuo_fragColor.a = 1.0;\n"
    "        }\n"
    "        else\n"
    "        {\n"
    "            // inside: alpha blended, staying fully transparent until the\n"
    "            // last FADE_START fraction of the radius, then easing strongly\n"
    "            // to opaque at the circle edge\n"
    "            const float FADE_START = 0.9;\n"
    "            float t = clamp((d / uCircleTrans.z - FADE_START) / (1.0 - FADE_START), 0.0, 1.0);\n"
    "            float fade = t * t * (3.0 - 2.0 * t);\n"
    "            xuo_fragColor.a = mix(xuo_fragColor.a, 1.0, fade);\n"
    "        }\n"
    "    }\n"
    "}\n";

typedef struct sbuf_
{
    char *data;
    size_t len;
    size_t cap;
} sbuf;

static void sb_append(sbuf *b, const char *s)
{
    const size_t n = strlen(s);
    if (b->len + n + 1 > b->cap)
    {
        size_t cap = b->cap ? b->cap : 1024;
        while (cap < b->len + n + 1)
        {
            cap *= 2;
        }
        char *grown = (char *)realloc(b->data, cap);
        if (grown == NULL)
        {
            return;
        }
        b->data = grown;
        b->cap = cap;
    }
    memcpy(b->data + b->len, s, n + 1);
    b->len += n;
}

static void sb_printf(sbuf *b, const char *fmt, int value)
{
    char tmp[256];
    snprintf(tmp, sizeof(tmp), fmt, value);
    sb_append(b, tmp);
}

static void sb_begin(sbuf *b, const glctx_shader_dialect *d)
{
    memset(b, 0, sizeof(*b));
    sb_append(b, "#version ");
    sb_append(b, d->version);
    sb_append(b, "\n");
    sb_append(b, d->header_extra);
    sb_append(b,
              "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
              "precision highp float;\n"
              "#else\n"
              "precision mediump float;\n"
              "#endif\n");
}

// the fragment stage writes this name in both dialects
static void sb_fragment_output(sbuf *b)
{
    sb_append(b, "out highp vec4 xuo_fragColor;\n");
}

void shader_src_init(void)
{
    if (s_vertex != NULL)
    {
        return;
    }

    const glctx_shader_dialect *d = glctx_shader_dialect_get();
    sbuf v;
    sb_begin(&v, d);
    for (int i = 0; i < 5; ++i)
    {
        if (strchr(d->attribute, '=') != NULL)
        {
            // desktop/ES3: layout (location = N) in
            sb_printf(&v, d->attribute, i);
            sb_append(&v, ") in ");
        }
        else
        {
            sb_append(&v, d->attribute); // attribute / in / varying
        }
        sb_append(&v, s_attributes[i]);
        sb_append(&v, "\n");
    }
    sb_append(&v,
              "uniform mat4 uProjectionView;\n"
              "uniform mat4 uModel;\n");
    sb_append(&v, d->out);
    sb_append(&v, "vec2 vUV;\n");
    sb_append(&v, d->out);
    sb_append(&v, "vec4 vColor;\n");
    sb_append(&v, d->out);
    sb_append(&v, "vec3 vNormal;\n");
    sb_append(&v, d->out);
    sb_append(&v, "float vLayer;\n");
    sb_append(&v, "void main()\n{\n");
    sb_append(&v, VERTEX_BODY);
    sb_append(&v, "\n}\n");
    s_vertex = v.data;

    sbuf f;
    sb_begin(&f, d);
    sb_append(&f, d->in);
    sb_append(&f, "vec2 vUV;\n");
    sb_append(&f, d->in);
    sb_append(&f, "vec4 vColor;\n");
    sb_append(&f, d->in);
    sb_append(&f, "vec3 vNormal;\n");
    sb_append(&f, d->in);
    sb_append(&f, "float vLayer;\n");
    sb_fragment_output(&f);
    sb_append(&f,
              "uniform sampler2D uTex;\n"
              "uniform sampler2DArray uTexArray;\n"
              "uniform int uAlphaTestEnabled;\n"
              "uniform float uAlphaRef;\n"
              "uniform int drawMode;\n"
              "uniform float colors[96];\n"
              "uniform vec2 uViewport;\n"
              "uniform vec3 uCircleTrans;\n"
              "\n"
              "vec4 sampleTex(vec2 uv, float layer)\n"
              "{\n"
              "   if (layer >= 0.0)\n"
              "   {\n"
              "       return texture(uTexArray, vec3(uv, layer));\n"
              "   }\n"
              "   return texture(uTex, uv);\n"
              "}\n"
              "\n"
              "void main()\n{\n");
    sb_append(&f, FRAGMENT_BODY);
    sb_append(&f, "\n}\n");
    s_fragment = f.data;
}

const char *shader_src_vertex(void)
{
    shader_src_init();
    return s_vertex;
}

const char *shader_src_fragment(void)
{
    shader_src_init();
    return s_fragment;
}

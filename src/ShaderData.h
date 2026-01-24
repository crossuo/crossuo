// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once
#include <external/gfx/gfx.h>

// clang-format off

static const char *g_vShader =
    GL_SHADER_HEADER
    GL_SHADER_ATTRIBUTE(0) "vec2 inPos;\n"
    GL_SHADER_ATTRIBUTE(1) "vec2 inUV;\n"
    GL_SHADER_ATTRIBUTE(2) "vec4 inColor;\n"
    GL_SHADER_ATTRIBUTE(3) "vec3 inNormal;\n"
    "uniform mat4 uProjectionView;\n"
    "uniform mat4 uModel;\n"
    GL_SHADER_OUT "vec2 vUV;\n"
    GL_SHADER_OUT "vec4 vColor;\n"
    GL_SHADER_OUT "vec3 vNormal;\n"
    "void main()\n"
    "{\n"
    "    vUV = inUV;\n"
    "    vColor = inColor;\n"
    "    vNormal = inNormal;\n"
    "    gl_Position = uProjectionView * uModel * vec4(inPos, 0.0, 1.0);\n"
    "}";

static const char *g_pShader =
    GL_SHADER_HEADER
    GL_SHADER_IN "vec2 vUV;\n"
    GL_SHADER_IN "vec4 vColor;\n"
    GL_SHADER_IN "vec3 vNormal;\n"
    "uniform sampler2D uTex;\n"
    "uniform int uAlphaTestEnabled;\n"
    "uniform float uAlphaRef;\n"
    "uniform int drawMode;\n"
    "uniform float colors[96];\n"
    "void main()\n"
    "{\n"
    "    vec4 textureColor = texture2D(uTex, vUV);\n"
    "    if (uAlphaTestEnabled != 0 && textureColor.a <= uAlphaRef) {\n"
    "        discard;\n"
    "    }\n"
    "    \n"
    "    if (textureColor.a != 0.0)\n"
    "    {\n"
    "        // SDM_LAND = 6 or SDM_LAND_COLORED = 7: apply lighting\n"
    "        if (drawMode == 6 || drawMode == 7)\n"
    "        {\n"
    "            vec3 lightDir = normalize(vec3(-1.0, -1.0, 0.5));\n"
    "            vec3 normal = normalize(vNormal);\n"
    "            float light = max(dot(normal, lightDir) + 0.5, 0.0);\n"
    "            \n"
    "            if (drawMode == 7) // SDM_LAND_COLORED\n"
    "            {\n"
    "                int index = int(textureColor.r * 31.875) * 3;\n"
    "                gl_FragColor = (vec4(colors[index], colors[index + 1], colors[index + 2], textureColor.a) * vColor) * light;\n"
    "            }\n"
    "            else // SDM_LAND\n"
    "            {\n"
    "                gl_FragColor = (textureColor * vColor) * light;\n"
    "            }\n"
    "        }\n"
    "        // SDM_COLORED = 1 || SDM_PARTIAL_HUE = 2\n"
    "        else if (drawMode == 1 || (drawMode == 2 && textureColor.r == textureColor.g && textureColor.r == textureColor.b))\n"
    "        {\n"
    "            int index = int(textureColor.r * 31.875) * 3;\n"
    "            gl_FragColor = vec4(colors[index], colors[index + 1], colors[index + 2], textureColor.a) * vColor;\n"
    "        }\n"
    "        // SDM_TEXT_COLORED = 4 || SDM_TEXT_COLORED_NO_BLACK = 3\n"
    "        else if (drawMode == 4 || (drawMode == 3 && textureColor.r > 0.04))\n"
    "        {\n"
    "            gl_FragColor = vec4(colors[90], colors[91], colors[92], textureColor.a) * vColor;\n"
    "        }\n"
    "        // SDM_SPECTRAL = 10, SDM_SPECIAL_SPECTRAL = 11, SDM_SHADOW = 12\n"
    "        else if (drawMode > 9)\n"
    "        {\n"
    "            float red = textureColor.r;\n"
    "            if (drawMode > 10)\n"
    "            {\n"
    "                if (drawMode > 11)\n"
    "                    red = 0.6;\n"
    "                else\n"
    "                    red *= 0.5;\n"
    "            }\n"
    "            else\n"
    "                red *= 1.5;\n"
    "            gl_FragColor = vec4(red, red, red, textureColor.a) * vColor;\n"
    "        }\n"
    "        else\n"
    "        {\n"
    "            gl_FragColor = textureColor * vColor;\n"
    "        }\n"
    "    }\n"
    "    else\n"
    "    {\n"
    "        gl_FragColor = textureColor * vColor;\n"
    "    }\n"
    "}";

static const char *g_Vert_ShaderData = g_vShader;
static const char *g_Frag_DeathShaderData = g_pShader;
static const char *g_Frag_LightShaderData = g_pShader;
static const char *g_Frag_FontShaderData = g_pShader;
static const char *g_Frag_ColorizerShaderData = g_pShader;
static const char *g_Frag_LandShaderData = g_pShader;

// clang-format on

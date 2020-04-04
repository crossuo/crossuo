// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#define SHADER_HEADER \
    "#version 100\n" \
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n" \
    "precision highp float;\n" \
    "#else\n" \
    "precision mediump float;\n" \
    "#endif\n"

static const char *g_vShader =
    SHADER_HEADER
    "attribute vec2 inPos;\n"
    "attribute vec2 inUV;\n"
    "attribute vec4 inColor;\n"
    "uniform mat4 uProjectionView;\n"
    "uniform mat4 uModel;\n"
    "varying vec2 vUV;\n"
    "varying vec4 vColor;\n"
    "void main()\n"
    "{\n"
    "    vUV = inUV;\n"
    "    vColor = inColor;\n"
    "    gl_Position = uProjectionView * uModel * vec4(inPos, 0.0, 1.0);\n"
    "}";

static const char *g_pShader =
    SHADER_HEADER
    "varying vec2 vUV;\n"
    "varying vec4 vColor;\n"
    "uniform sampler2D uTex;\n"
    "void main()\n"
    "{\n"
    "    vec2 x = vec2(1.0, 1.0) + vUV;\n"
    "    vec4 c = texture2D(uTex, vUV);\n"
    "    vec4 c2 = vColor;\n"
    "    c2.a = c.a;\n"
    "    gl_FragColor = c2;\n"
    "}";


#if defined(USE_GLES)

static const char *g_Vert_ShaderData = g_vShader;
static const char *g_Frag_DeathShaderData = g_pShader;
static const char *g_Frag_LightShaderData = g_pShader;
static const char *g_Frag_FontShaderData = g_pShader;
static const char *g_Frag_ColorizerShaderData = g_pShader;

#endif


#if defined(USE_GL)

static const char *g_Vert_ShaderData =
    SHADER_HEADER
    "varying vec3 l;\n"
    "varying vec3 n;\n"
    "uniform int drawMode;\n"
    "void main(void)\n"
    "{\n"
    "	if (drawMode > 5)\n"
    "	{\n"
    "		l = normalize(vec3(gl_LightSource[0].position));\n"
    "		n = normalize(gl_NormalMatrix * gl_Normal);\n"
    "	}\n"
    "	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
    "	gl_TexCoord[0] = gl_MultiTexCoord0;\n"
    "	gl_FrontColor = gl_Color;\n"
    "}";

static const char *g_Frag_DeathShaderData =
    SHADER_HEADER
    "varying vec3 l;\n"
    "varying vec3 n;\n"
    "uniform sampler2D usedTexture;\n"
    "uniform int drawMode;\n"
    "void main(void)\n"
    "{\n"
    "	vec4 textureColor = texture2D(usedTexture, gl_TexCoord[0].xy);\n"
    "	if (textureColor.a != 0.0)\n"
    "	{\n"
    "		if (drawMode > 5)\n"
    "		{\n"
    "			vec3 n2 = normalize(n);\n"
    "			vec3 l2 = normalize(l);\n"
    "			vec4 normal = vec4(max(dot(n2, l2) + 0.5, 0.0));\n"
    "			textureColor = textureColor * normal;\n"
    "		}\n"
    "		float red = (textureColor.r * 0.6 + textureColor.g * 0.05);\n"
    "		textureColor = vec4(red, red, red, textureColor.a);\n"
    "	}\n"
    "	gl_FragColor = textureColor;\n"
    "}";

static const char *g_Frag_LightShaderData =
    SHADER_HEADER
    "uniform sampler2D usedTexture;\n"
    "uniform int drawMode;\n"
    "uniform float colors[96];\n"
    "void main(void)\n"
    "{\n"
    "	vec4 textureColor = texture2D(usedTexture, gl_TexCoord[0].xy);\n"
    "	if (textureColor.a != 0.0 && drawMode == 1)\n"
    "	{\n"
    "		int index = int(textureColor.r * 7.96875) * 3;\n"
    "		gl_FragColor = (textureColor * vec4(colors[index], colors[index + 1], colors[index + 2], 1.0)) * 3.0;\n"
    "	}\n"
    "	else\n"
    "		gl_FragColor = textureColor;\n"
    "}";

static const char *g_Frag_FontShaderData =
    SHADER_HEADER
    "uniform sampler2D usedTexture;\n"
    "uniform int drawMode;\n"
    "uniform float colors[96];\n"
    "void main(void)\n"
    "{\n"
    "	vec4 textureColor = texture2D(usedTexture, gl_TexCoord[0].xy);\n"
    "	if (textureColor.a != 0.0)\n"
    "	{\n"
    "		if (drawMode == 1 || (drawMode == 2 && textureColor.r == textureColor.g && textureColor.r == textureColor.b))\n"
    "		{\n"
    "			int index = int(textureColor.r * 31.875) * 3;\n"
    "			gl_FragColor = vec4(colors[index], colors[index + 1], colors[index + 2], textureColor.a) * gl_Color;\n"
    "		}\n"
    "		else if (drawMode == 4 || (drawMode == 3 && textureColor.r > 0.04))\n"
    "		{\n"
    "			gl_FragColor = vec4(colors[90], colors[91], colors[92], textureColor.a) * gl_Color;\n"
    "		}\n"
    "		else\n"
    "			gl_FragColor = textureColor * gl_Color;\n"
    "	}\n"
    "	else\n"
    "		gl_FragColor = textureColor * gl_Color;\n"
    "}";

static const char *g_Frag_ColorizerShaderData =
    SHADER_HEADER
    "varying vec3 l;\n"
    "varying vec3 n;\n"
    "uniform sampler2D usedTexture;\n"
    "uniform int drawMode;\n"
    "uniform float colors[96];\n"
    "void main(void)\n"
    "{\n"
    "	vec4 textureColor = texture2D(usedTexture, gl_TexCoord[0].xy);\n"
    "	if (textureColor.a != 0.0)\n"
    "	{\n"
    "		if (drawMode == 1 || (drawMode == 2 && textureColor.r == textureColor.g && textureColor.r == textureColor.b))\n"
    "		{\n"
    "			int index = int(textureColor.r * 31.875) * 3;\n"
    "			gl_FragColor = vec4(colors[index], colors[index + 1], colors[index + 2], textureColor.a) * gl_Color;\n"
    "		}\n"
    "		else if (drawMode > 5)\n"
    "		{\n"
    "			if (drawMode > 9)\n"
    "			{\n"
    "				float red = textureColor.r;\n"
    "				if (drawMode > 10)\n"
    "				{\n"
    "					if (drawMode > 11)\n"
    "						red = 0.6;\n"
    "					else\n"
    "						red *= 0.5;\n"
    "				}\n"
    "				else\n"
    "					red *= 1.5;\n"
    "				gl_FragColor = vec4(red, red, red, textureColor.a) * gl_Color;\n"
    "			}\n"
    "			else\n"
    "			{\n"
    "				vec3 n2 = normalize(n);\n"
    "				vec3 l2 = normalize(l);\n"
    "				vec4 normal = vec4(max(dot(n2, l2) + 0.5, 0.0));\n"
    "				if (drawMode > 6)\n"
    "				{\n"
    "					int index = int(textureColor.r * 31.875) * 3;\n"
    "					gl_FragColor = (vec4(colors[index], colors[index + 1], colors[index + 2], textureColor.a) * gl_Color) * normal;\n"
    "				}\n"
    "				else\n"
    "					gl_FragColor = (textureColor * gl_Color) * normal;\n"
    "			}\n"
    "		}\n"
    "		else\n"
    "			gl_FragColor = textureColor * gl_Color;\n"
    "	}\n"
    "	else\n"
    "		gl_FragColor = textureColor * gl_Color;\n"
    "}";
#endif

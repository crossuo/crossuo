// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once
#include <external/gfx/gfx.h>
#include <external/gfx/shaderbuilder.h>
#include <xuocore/enumlist.h>

static_assert(SDM_CHECKTRANS_BASE == 100, "the checktrans base is hardcoded in the shader");

// The GLSL sources are assembled at run time by the glctx flavour (desktop
// 3.30 vs ES 3.00): see external/gfx/shaderbuilder.c. These accessors keep the
// names the renderer already uses.
#define g_vShader shader_src_vertex()
#define g_pShader shader_src_fragment()

#define g_Vert_ShaderData shader_src_vertex()
#define g_Frag_DeathShaderData shader_src_fragment()
#define g_Frag_LightShaderData shader_src_fragment()
#define g_Frag_FontShaderData shader_src_fragment()
#define g_Frag_ColorizerShaderData shader_src_fragment()
#define g_Frag_LandShaderData shader_src_fragment()

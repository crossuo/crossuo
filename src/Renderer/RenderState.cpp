// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Everton Fernando Patitucci da Silva

#if defined(NEW_RENDERER_ENABLED) && (defined(USE_GL3) || defined(USE_GLES))
#include "../Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "../Renderer/RenderInternal.h"
#include "Debug/RenderDebug.h"
#include "../Utility/PerfMarker.h"
#include <external/gfx/gfx.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <assert.h>
#include <string.h> // memcmp, memcpy

// Exact float comparison for render-state caching: any bit difference means
// the state changed and GL state must be re-issued. Bit-level compare keeps
// the exact-match semantics while staying warning-free (-Wfloat-equal).
static bool f32eq(float a, float b)
{
    static_assert(sizeof(float) == sizeof(uint32_t), "requires 32-bit float");
    uint32_t ba, bb;
    memcpy(&ba, &a, sizeof(ba));
    memcpy(&bb, &b, sizeof(bb));
    return ba == bb;
}
#define countof(xarray) (sizeof(xarray) / sizeof(xarray[0]))

static ShaderPipeline g_pipeline = {};

// Helper function to set up cached GL state before drawing
// Returns true if any state changed, false if all state was already set
bool RenderState_SetupCachedState(RenderState *state, const glm::mat4 &modelMatrix)
{
    bool stateChanged = false;

    GL_CHECK(glBindVertexArray(g_drawVAO));
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, g_drawVBO));

    GL_CHECK(glUseProgram(_pProg));
    // Bind shader program (always bind to ensure it's active)
    if (state->currentProgram != _pProg)
    {
        GL_CHECK(glUseProgram(_pProg));
        state->currentProgram = _pProg;
        g_pipeline.program = _pProg;
        stateChanged = true;
    }

    // Upload alpha test uniforms if needed
    GL_CHECK(glUniform1i(_uAlphaTestEnabled, state->alphaTest.enabled ? 1 : 0));
    GL_CHECK(glUniform1f(_uAlphaRef, state->alphaTest.alphaRef));

    // Upload model matrix if changed
    if (!state->modelMatrixCached ||
        memcmp(state->cachedModelMatrix, &modelMatrix[0][0], sizeof(state->cachedModelMatrix)) != 0)
    {
        GL_CHECK(glUniformMatrix4fv(_uModel, 1, false, glm::value_ptr(modelMatrix)));
        memcpy(state->cachedModelMatrix, &modelMatrix[0][0], sizeof(state->cachedModelMatrix));
        state->modelMatrixCached = true;
        stateChanged = true;
    }
    return stateChanged;
}

// Helper function to reset all render states to defaults after a draw call
void RenderState_ResetAllStates(RenderState *state)
{
    if (g_rendererDebugForceStateReset)
    {
        // Reset blend state
        //RenderState_SetBlendEnabled(state, false);
        //state->currentDrawMode = 1;
/*
        // Reset color to white
        RenderState_SetColor(state, g_ColorWhite);

        // Reset alpha test to default
        RenderState_SetAlphaTest(state, true, AlphaTestFunc::AlphaTestFunc_Greater, 0.f);

        // Reset stencil state
        RenderState_SetStencilEnabled(state, false);

        // Reset depth state
        RenderState_SetDepthEnabled(state, flase);

        // Reset scissor state
        RenderState_SetScissor(state, false, 0, 0, 0, 0);

        // Reset color mask
        RenderState_SetColorMask(state, ColorMask::ColorMask_All);

        // Reset shader pipeline
        RenderState_DisableShaderPipeline(state);

        // Reset GL program
        //GL_CHECK(glUseProgram(0));

        // Reset texture to default
        RenderState_SetTexture(state, TextureType::TextureType_Texture2D, RENDER_TEXTUREHANDLE_INVALID);

        // Reset framebuffer to default
        RenderState_SetFrameBuffer(state, {});

        // Reset uniform cache
        state->uniformCache = RenderStateUniformCache{};

        memset(state->currentColors, 0, sizeof(state->currentColors));
        state->currentProgram = 0;
        state->modelMatrixCached = false;
        */
    }
}

bool RenderState_FlushState(RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);
    RenderState_SetTexture(state, state->texture.type, state->texture.texture, true);
    RenderState_SetFrameBuffer(state, state->framebuffer, true);
    RenderState_SetBlend(
        state,
        state->blend.enabled,
        state->blend.src,
        state->blend.dst,
        state->blend.equation,
        true);
    RenderState_SetAlphaTest(
        state, state->alphaTest.enabled, state->alphaTest.func, state->alphaTest.alphaRef, true);
    RenderState_SetStencil(
        state,
        state->stencil.enabled,
        state->stencil.func,
        state->stencil.ref,
        state->stencil.mask,
        state->stencil.stencilFail,
        state->stencil.depthFail,
        state->stencil.bothFail,
        true);
    RenderState_SetDepth(state, state->depth.enabled, state->depth.func, true);
    RenderState_SetColorMask(state, state->colorMask, true);
    RenderState_SetColor(state, state->color, true);
    RenderState_SetClearColor(state, state->clearColor, true);
    RenderState_SetScissor(
        state,
        state->scissor.enabled,
        state->scissor.x,
        state->scissor.y,
        state->scissor.width,
        state->scissor.height);

    glm::mat4 identity(1.0f);
    GL_CHECK(glUseProgram(_pProg));
    GL_CHECK(glUniformMatrix4fv(_uModel, 1, false, glm::value_ptr(identity)));

    RenderState_SetDrawMode(state, 0, true);
    //RenderState_SetShaderPipeline(state, &state->pipeline, true);
    // FIXME uniform cache is not applied during flush, not sure if it should be applied or if the behavior
    // should be clear
    // TODO add a compile-time assert to ensure any newly added command is applied or properly ignored here

    return true;
}

bool RenderState_SetAlphaTest(
    RenderState *state, bool enabled, AlphaTestFunc func, float ref, bool forced)
{
    static GLenum s_alphaTestfuncToOGLFunc[] = {
        GL_NEVER,    // AlphaTest_NeverPass
        GL_ALWAYS,   // AlphaTest_AlwaysPass
        GL_EQUAL,    // AlphaTest_Equal
        GL_NOTEQUAL, // AlphaTest_Different
        GL_LESS,     // AlphaTest_Less
        GL_LEQUAL,   // AlphaTest_LessOrEqual
        GL_GREATER,  // AlphaTest_Greater
        GL_GEQUAL,   // AlphaTest_GreaterOrEqual
    };

    static_assert(
        countof(s_alphaTestfuncToOGLFunc) == AlphaTestFunc::AlphaTestFunc_Count,
        "missing alpha test funcs");

    bool changed = false;
    if (state->alphaTest.enabled != enabled || forced)
    {
        changed = true;
        state->alphaTest.enabled = enabled;
        // For GL3/GLES, we'll set the uniform when the program is used
        // Just store the state for now
    }

    auto differentFuncOrRef = [&]() -> bool {
        return state->alphaTest.func != func || !f32eq(state->alphaTest.alphaRef, ref);
    };

    if (enabled &&
        (differentFuncOrRef() || (forced && func != AlphaTestFunc::AlphaTestFunc_Invalid)))
    {
        changed = true;
        state->alphaTest.func = func;
        state->alphaTest.alphaRef = ref;
        // For GL3/GLES, we'll set the uniform when the program is used
        // Just store the state for now
    }

    return changed;
}

bool RenderState_SetBlend(
    RenderState *state,
    bool enabled,
    BlendFactor src,
    BlendFactor dst,
    BlendEquation equation,
    bool forced)
{
    static GLenum s_blendFactorToOGLEnum[] = {
        GL_ZERO,                // Zero
        GL_ONE,                 // One
        GL_ONE_MINUS_SRC_ALPHA, // OneMinusSrcAlpha
        GL_ONE_MINUS_SRC_COLOR, // OneMinusSrcColor
        GL_SRC_COLOR,           // SrcColor
        GL_SRC_ALPHA,           // SrcAlpha
        GL_DST_COLOR,           // DstColor
    };

    static GLenum s_blendEquationToOGLEnum[] = {
        GL_FUNC_ADD,              // Add
        GL_FUNC_REVERSE_SUBTRACT, // ReverseSubtract
    };

    static_assert(
        countof(s_blendFactorToOGLEnum) == BlendFactor::BlendFactor_Count,
        "missing blend factor mapping");
    static_assert(
        countof(s_blendEquationToOGLEnum) == BlendEquation::BlendEquation_Count,
        "missing blend equation mapping");

    bool changed = false;
    if (forced || state->blend.enabled != enabled || state->blend.src != src || state->blend.dst != dst || state->blend.equation != equation)
    {
        changed = true;
        state->blend.enabled = enabled;
        if (enabled)
        {
            GL_CHECK(glEnable(GL_BLEND));
            state->blend.src = src;
            state->blend.dst = dst;
            GL_CHECK(glBlendFunc(s_blendFactorToOGLEnum[src], s_blendFactorToOGLEnum[dst]));
            state->blend.equation = equation;
            GL_CHECK(glBlendEquation(s_blendEquationToOGLEnum[equation]));
        }
        else
        {
            GL_CHECK(glDisable(GL_BLEND));
        }
    }

    return changed;
}

bool RenderState_SetBlendEnabled(RenderState *state, bool enabled, bool forced)
{
    if (forced || state->blend.enabled != enabled)
    {
        RENDER_STATE_LOG_BLEND(state->blend.enabled, enabled);
        if (!forced)
        {
            RenderDebug_LogStackTrace("  BLEND STATE CHANGE");
        }
        state->blend.enabled = enabled;
        if (enabled)
        {
            GL_CHECK(glEnable(GL_BLEND));
        }
        else
        {
            GL_CHECK(glDisable(GL_BLEND));
        }
        return true;
    }
    return false;
}

bool RenderState_SetDepth(RenderState *state, bool enabled, DepthFunc func, bool forced)
{
    static GLenum s_depthFuncToOGLFunc[] = {
        GL_NEVER,    // DepthFunc::DepthFunc_NeverPass,
        GL_ALWAYS,   // DepthFunc::DepthFunc_AlwaysPass,
        GL_EQUAL,    // DepthFunc::DepthFunc_Equal,
        GL_NOTEQUAL, // DepthFunc::DepthFunc_Different,
        GL_LESS,     // DepthFunc::DepthFunc_Less,
        GL_LEQUAL,   // DepthFunc::DepthFunc_LessOrEqual,
        GL_GREATER,  // DepthFunc::DepthFunc_Greater,
        GL_GEQUAL,   // DepthFunc::DepthFunc_GreaterOrEqual
    };

    static_assert(
        countof(s_depthFuncToOGLFunc) == DepthFunc::DepthFunc_Count, "missing depth func");

    if (forced || state->depth.enabled != enabled || (enabled && state->depth.func != func))
    {
        state->depth.enabled = enabled;
        state->depth.func = func;
        if (enabled)
        {
            GL_CHECK(glEnable(GL_DEPTH_TEST));
            GL_CHECK(glDepthFunc(s_depthFuncToOGLFunc[func]));
        }
        else
        {
            GL_CHECK(glDisable(GL_DEPTH_TEST));
        }
        return true;
    }

    return false;
}

bool RenderState_SetDepthEnabled(RenderState *state, bool enabled, bool forced)
{
    if (forced || state->depth.enabled != enabled)
    {
        state->depth.enabled = enabled;
        if (enabled)
        {
            GL_CHECK(glEnable(GL_DEPTH_TEST));
        }
        else
        {
            GL_CHECK(glDisable(GL_DEPTH_TEST));
        }
        return true;
    }

    return false;
}

bool RenderState_SetStencilEnabled(RenderState *state, bool enabled, bool forced)
{
    if (forced || state->stencil.enabled != enabled)
    {
        state->stencil.enabled = enabled;
        if (enabled)
        {
            GL_CHECK(glEnable(GL_STENCIL_TEST));
        }
        else
        {
            GL_CHECK(glDisable(GL_STENCIL_TEST));
        }

        return true;
    }

    return false;
}

bool RenderState_SetStencil(
    RenderState *state,
    bool enabled,
    StencilFunc func,
    uint32_t ref,
    uint32_t mask,
    StencilOp stencilFail,
    StencilOp depthFail,
    StencilOp bothFail,
    bool forced)
{
    static GLenum s_stencilFuncToOGLFunc[] = {
        GL_NEVER,   // StencilFunc::StencilFunc_NeverPass
        GL_ALWAYS,  // StencilFunc::StencilFunc_AlwaysPass
        GL_LESS,    // StencilFunc::StencilFunc_Less
        GL_LEQUAL,  // StencilFunc::StencilFunc_LessOrEqual
        GL_GREATER, // StencilFunc::StencilFunc_Greater
        GL_GEQUAL,  // StencilFunc::StencilFunc_GreaterOrEqual
        GL_EQUAL,   // StencilFunc::StencilFunc_Equal
        GL_NOTEQUAL // StencilFunc::StencilFunc_Different
    };

    static GLenum s_stencilOpToOGLOp[] = {
        GL_KEEP,      // StencilOp::StencilOp_Keep
        GL_ZERO,      // StencilOp::StencilOp_Zero
        GL_REPLACE,   // StencilOp::StencilOp_Replace
        GL_INCR,      // StencilOp::StencilOp_IncrementClamp
        GL_INCR_WRAP, // StencilOp::StencilOp_IncrementWrap
        GL_DECR,      // StencilOp::StencilOp_DecrementClamp,
        GL_DECR_WRAP, // StencilOp::StencilOp_DecrementWrap,
        GL_INVERT     // StencilOp::StencilOp_Invert
    };

    static_assert(
        countof(s_stencilFuncToOGLFunc) == StencilFunc::StencilFunc_Count, "missing stencil func");
    static_assert(countof(s_stencilOpToOGLOp) == StencilOp::StencilOp_Count, "missing stencil op");

    if (forced || state->stencil.enabled != enabled ||
        (enabled && (state->stencil.func != func || state->stencil.stencilFail != stencilFail ||
                     state->stencil.depthFail != depthFail || state->stencil.bothFail != bothFail ||
                     state->stencil.ref != ref || state->stencil.mask != mask)))
    {
        state->stencil.enabled = enabled;
        state->stencil.func = func;
        state->stencil.stencilFail = stencilFail;
        state->stencil.depthFail = depthFail;
        state->stencil.bothFail = bothFail;
        state->stencil.ref = ref;
        state->stencil.mask = mask;
        if (enabled)
        {
            GL_CHECK(glEnable(GL_STENCIL_TEST));
            GL_CHECK(glStencilFunc(s_stencilFuncToOGLFunc[func], ref, mask));
            GL_CHECK(glStencilOp(
                s_stencilOpToOGLOp[stencilFail],
                s_stencilOpToOGLOp[depthFail],
                s_stencilOpToOGLOp[bothFail]));
        }
        else
        {
            GL_CHECK(glDisable(GL_STENCIL_TEST));
        }
        return true;
    }

    return false;
}

bool RenderState_SetColorMask(RenderState *state, ColorMask mask, bool forced)
{
    if (forced || state->colorMask != mask)
    {
        state->colorMask = mask;
        GL_CHECK(glColorMask(
            mask & ColorMask::ColorMask_Red ? GL_TRUE : GL_FALSE,
            mask & ColorMask::ColorMask_Green ? GL_TRUE : GL_FALSE,
            mask & ColorMask::ColorMask_Blue ? GL_TRUE : GL_FALSE,
            mask & ColorMask::ColorMask_Alpha ? GL_TRUE : GL_FALSE));
        return true;
    }

    return false;
}

bool RenderState_SetDrawMode(RenderState *state, int drawMode, bool forced)
{
    if (forced || state->currentDrawMode != drawMode)
    {
        state->currentDrawMode = drawMode;
        GL_CHECK(glUniform1i(_uDrawMode, drawMode));
        return true;
    }

    return false;
}

bool RenderState_SetColor(RenderState *state, float4 color, bool forced)
{
    if (forced || state->color != color)
    {
        state->color = color;
        memcpy(state->color.rgba, color.rgba, sizeof(state->color.rgba));
        return true;
    }

    return false;
}

bool RenderState_SetClearColor(RenderState *state, float4 color, bool forced)
{
    if (forced || state->clearColor != color)
    {
        state->clearColor = color;
        memcpy(state->clearColor.rgba, color.rgba, sizeof(state->clearColor.rgba));
        GL_CHECK(glClearColor(color[0], color[1], color[2], color[3]));
        return true;
    }

    return false;
}

bool RenderState_SetColorPalette(RenderState *state, const float *colors, bool forced)
{
    constexpr int paletteSize = 96;
    assert(sizeof(state->currentColors) == paletteSize * sizeof(float));
    if (memcmp(state->currentColors, colors, paletteSize * sizeof(float)) != 0)
    {
        memcpy(state->currentColors, colors, paletteSize * sizeof(float));
        GL_CHECK(glUniform1fv(_uColors, paletteSize, colors));
        return true;
    }

    return false;
}

bool RenderState_SetShaderUniform(
    RenderState *state, uint32_t id, const void *value, ShaderUniformType type, bool forced)
{
    // FIXME leave this commented until shader usage is consistent throughout drawables
    // assert(state->pipeline.program != RENDER_SHADERPROGRAM_INVALID);
    if (state->pipeline.program == RENDER_SHADERPROGRAM_INVALID)
    {
        return false;
    }

    assert(id < state->pipeline.uniformCount);
    auto location = state->pipeline.uniforms[id].location;

    auto uniformSize = Render_ShaderUniformTypeToSize(type);

    if (forced || (!state->uniformCache.dataSet[id] ||
                   memcmp(state->uniformCache.data[id], value, uniformSize) != 0))
    {
        switch (type)
        {
            case ShaderUniformType::ShaderUniformType_Int1:
            {
                auto typedValue = *(GLint *)value;
                GL_CHECK(glUniform1i(location, typedValue));
                break;
            }

            // Large uniform types
            case ShaderUniformType::ShaderUniformType_Float1V:
            default:
            {
                assert(false);
                return false;
            }
        }

        assert(uniformSize < RENDERSTATE_SHADER_UNIFORMDATA_SIZE);
        if (uniformSize < RENDERSTATE_SHADER_UNIFORMDATA_SIZE)
        {
            memcpy(state->uniformCache.data[id], value, uniformSize);
            state->uniformCache.dataSet[id] = true;
        }

        return true;
    }

    return false;
}

bool RenderState_SetShaderLargeUniform(
    RenderState *state,
    uint32_t id,
    void *value,
    uint32_t count,
    ShaderUniformType type,
    bool forced)
{
    // FIXME leave this commented until shader usage is consistent throughout drawables
    // assert(state->pipeline.program != RENDER_SHADERPROGRAM_INVALID);
    // if (state->pipeline.program == RENDER_SHADERPROGRAM_INVALID)
    // {
    //     return false;
    // }

    assert(id < state->pipeline.uniformCount);
    auto location = state->pipeline.uniforms[id].location;

    switch (type)
    {
        case ShaderUniformType::ShaderUniformType_Float1V:
        {
            auto typedValue = (GLfloat *)value;
            GL_CHECK(glUniform1fv(location, count, typedValue));
            break;
        }

        default:
        {
            assert(false);
            return false;
        }
    }

    return true;
}

bool RenderState_SetShaderPipeline(RenderState *state, ShaderPipeline *pipeline, bool forced)
{
    assert(pipeline);
    if (state->pipeline.program != pipeline->program || forced)
    {
        GL_CHECK(glUseProgram(pipeline->program));
        memcpy(&state->pipeline, pipeline, sizeof(*pipeline));
        state->uniformCache = RenderStateUniformCache{};

        return true;
    }

    return false;
}

bool RenderState_DisableShaderPipeline(RenderState *state, bool forced)
{
    if (state->pipeline.program != RENDER_SHADERPROGRAM_INVALID || forced)
    {
        GL_CHECK(glUseProgram(0));
        state->pipeline = ShaderPipeline{};
        state->uniformCache = RenderStateUniformCache{};

        return true;
    }
    return false;
}

bool RenderState_SetTexture(
    RenderState *state, TextureType type, texture_handle_t texture, bool forced)
{
    auto textureTypeToOGLType = [](TextureType type) {
        switch (type)
        {
            case TextureType::TextureType_Texture2D:
            case TextureType::TextureType_Texture2D_Mipmapped:
            {
                return GL_TEXTURE_2D;
            }

            default:
            {
                assert(false);
                break;
            }
        }
        return 0;
    };

    if (state->texture.texture != texture || state->texture.type != type || forced)
    {
        state->texture.texture = texture;
        state->texture.type = type;
        if (texture == RENDER_TEXTUREHANDLE_INVALID)
        {
#if defined(USE_GLES) || defined(USE_GL3)
            // Bind default white texture for GL3/GLES to support vertex colors
            GL_CHECK(glBindTexture(textureTypeToOGLType(type), _defaultTex));
#endif
        }
        else
        {
            GL_CHECK(glBindTexture(textureTypeToOGLType(type), texture));
            if (type == TextureType::TextureType_Texture2D_Mipmapped)
            {
                GL_CHECK(glGenerateMipmap(GL_TEXTURE_2D));
            }
        }

        return true;
    }

    return false;
}

bool RenderState_SetFrameBuffer(RenderState *state, frame_buffer_t fb, bool forced)
{
    if (forced || (state->framebuffer.handle != fb.handle || state->framebuffer.texture != fb.texture))
    {
        if (fb.handle != RENDER_FRAMEBUFFER_INVALID)
        {
            GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fb.handle));
            GL_CHECK(glBindTexture(GL_TEXTURE_2D, fb.texture));
        }
        else
        {
            GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        }
        state->framebuffer = fb;
        return true;
    }
    return false;
}

bool RenderState_SetViewParams(
    RenderState *state,
    int scene_x,
    int scene_y,
    int scene_width,
    int scene_height,
    int window_width,
    int window_height,
    int camera_nearZ,
    int camera_farZ,
    float scene_scale,
    bool proj_flipped_y,
    bool forced)
{
    const int right = scene_x + scene_width;
    const int needed_height = scene_y + scene_height;
    const int bottom = window_height - needed_height;

    if (forced ||
        state->viewport.left != scene_x ||
        state->viewport.right != right ||
        state->viewport.bottom != bottom ||
        state->viewport.top != scene_y ||
        state->viewport.nearZ != camera_nearZ ||
        state->viewport.farZ != camera_farZ ||
        !f32eq(state->viewport.scale, scene_scale) ||
        state->viewport.proj_flipped_y != proj_flipped_y)
    {
        ScopedPerfMarker(__FUNCTION__);

        state->viewport.left = scene_x;
        state->viewport.right = right;
        state->viewport.bottom = bottom;
        state->viewport.top = scene_y;
        state->viewport.nearZ = camera_nearZ;
        state->viewport.farZ = camera_farZ;
        state->viewport.scale = scene_scale;
        state->viewport.proj_flipped_y = proj_flipped_y;

        const float scaledRight = right * scene_scale;
        const float scaledLeft = scene_x * scene_scale - (scaledRight - right);
        const float scaledBottom = (scene_y + scene_height) * scene_scale;
        const float scaledTop = scene_y * scene_scale - (scaledBottom - (scene_y + scene_height));

        glViewport(scene_x, bottom, scene_width, scene_height);
        // TODO: gles - projection ortho view parms
        const auto projection = glm::ortho(
            scaledLeft,
            scaledRight,
            proj_flipped_y ? scaledTop : scaledBottom,
            proj_flipped_y ? scaledBottom : scaledTop,
            float(camera_nearZ),
            float(camera_farZ));
        GL_CHECK(glUseProgram(_pProg));
        GL_CHECK(glUniformMatrix4fv(_uProjectionView, 1, false, glm::value_ptr(projection)));
        return true;
    }
    return false;
}

bool RenderState_SetModelViewTranslation(RenderState *state, float3 pos, bool forced)
{
    // Accumulate translation in state for GL3/GLES (like glTranslatef does)
    state->modelTranslation.rgb[0] += pos[0];
    state->modelTranslation.rgb[1] += pos[1];
    state->modelTranslation.rgb[2] += pos[2];
    return true;
}

bool RenderState_SetScissor(
    RenderState *state, bool enabled, int x, int y, uint32_t width, uint32_t height, bool forced)
{
    bool changed = false;
    if (forced || (state->scissor.enabled != enabled))
    {
        changed = true;
        state->scissor.enabled = enabled;
        if (enabled)
        {
            GL_CHECK(glEnable(GL_SCISSOR_TEST));
        }
        else
        {
            GL_CHECK(glDisable(GL_SCISSOR_TEST));
        }
    }

    if (forced || (state->scissor.x != x || state->scissor.y != y ||
                   state->scissor.width != width || state->scissor.height != height))
    {
        changed = true;
        state->scissor.x = x;
        state->scissor.y = y;
        state->scissor.width = width;
        state->scissor.height = height;
        GL_CHECK(glScissor(x, y, width, height));
    }
    return changed;
}
#endif // #if defined(NEW_RENDERER_ENABLED) && (defined(USE_GL3) || defined(USE_GLES))

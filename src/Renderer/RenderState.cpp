#include "Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "Renderer/RenderInternal.h"
#include "Utility/PerfMarker.h"

bool RenderState_FlushState(RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);
    RenderState_SetTexture(state, state->texture.type, state->texture.texture, true);
    // FIXME epatitucci
    // RenderState_SetFrameBuffer(state, state->framebuffer, true);
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

    glLoadIdentity();

    // RenderState_SetShaderPipeline(state, &state->pipeline, true);
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
        if (enabled)
        {
            glEnable(GL_ALPHA_TEST);
        }
        else
        {
            glDisable(GL_ALPHA_TEST);
        }
    }

    auto differentFuncOrRef = [&]() -> bool {
        return state->alphaTest.func != func || state->alphaTest.alphaRef != ref;
    };

    if (enabled && differentFuncOrRef() || (forced && func != AlphaTestFunc::AlphaTestFunc_Invalid))
    {
        changed = true;
        state->alphaTest.func = func;
        state->alphaTest.alphaRef = ref;
        glAlphaFunc(s_alphaTestfuncToOGLFunc[func], ref);
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
    if (state->blend.enabled != enabled || forced)
    {
        changed = true;
        state->blend.enabled = enabled;
        if (enabled)
        {
            glEnable(GL_BLEND);
        }
        else
        {
            glDisable(GL_BLEND);
        }
    }

    if (enabled && (state->blend.src != src || state->blend.dst != dst) ||
        (forced && (state->blend.src != BlendFactor::BlendFactor_Invalid &&
                    state->blend.dst != BlendFactor::BlendFactor_Invalid)))
    {
        changed = true;
        state->blend.src = src;
        state->blend.dst = dst;
        glBlendFunc(s_blendFactorToOGLEnum[src], s_blendFactorToOGLEnum[dst]);
    }

    if (enabled && (forced || (state->blend.equation != equation)))
    {
        assert(state->blend.equation != BlendEquation::BlendEquation_Invalid);
        changed = true;
        state->blend.equation = equation;
        glBlendEquation(s_blendEquationToOGLEnum[equation]);
    }

    return changed;
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
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(s_depthFuncToOGLFunc[func]);
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }
        return true;
    }

    return false;
}

bool RenderState_SetDepthEnabled(RenderState *state, bool enabled, bool forced)
{
    if (forced || state->depth.enabled != enabled)
    {
        if (enabled)
        {
            glEnable(GL_DEPTH_TEST);
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }
        return true;
    }

    return false;
}

bool RenderState_SetStencilEnabled(RenderState *state, bool enabled, bool forced)
{
    if (forced || state->stencil.enabled != enabled)
    {
        if (enabled)
        {
            glEnable(GL_STENCIL_TEST);
        }
        else
        {
            glDisable(GL_STENCIL_TEST);
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
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(s_stencilFuncToOGLFunc[func], ref, mask);
            glStencilOp(
                s_stencilOpToOGLOp[stencilFail],
                s_stencilOpToOGLOp[depthFail],
                s_stencilOpToOGLOp[bothFail]);
        }
        else
        {
            glDisable(GL_STENCIL_TEST);
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
        glColorMask(
            mask & ColorMask::ColorMask_Red ? GL_TRUE : GL_FALSE,
            mask & ColorMask::ColorMask_Green ? GL_TRUE : GL_FALSE,
            mask & ColorMask::ColorMask_Blue ? GL_TRUE : GL_FALSE,
            mask & ColorMask::ColorMask_Alpha ? GL_TRUE : GL_FALSE);
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

        glColor4f(state->color[0], state->color[1], state->color[2], state->color[3]);
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

        glClearColor(color[0], color[1], color[2], color[3]);
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
                glUniform1i(location, typedValue);
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
            glUniform1fv(location, count, typedValue);
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
        glUseProgram(pipeline->program);
        memcpy(&state->pipeline, pipeline, sizeof(*pipeline));
        state->uniformCache = RenderStateUniformCache{};

        return true;
    }

    return false;
}

bool RenderState_DisableShaderPipeline(RenderState *state, bool forced)
{
    // if (state->pipeline.program != RENDER_SHADERPROGRAM_INVALID || forced)
    {
        glUseProgram(0);
        state->pipeline = ShaderPipeline{};
        state->uniformCache = RenderStateUniformCache{};

        return true;
    }

    // return false;
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
            // TODO bind null?
        }
        else
        {
            glBindTexture(textureTypeToOGLType(type), texture);
            if (type == TextureType::TextureType_Texture2D_Mipmapped)
            {
                glGenerateMipmap(GL_TEXTURE_2D);
            }
        }

        return true;
    }

    return false;
}

bool RenderState_SetFrameBuffer(RenderState *state, frame_buffer_t fb, bool forced)
{
    // TODO epatitucci FIXME
    // if (forced || (memcmp(&state->framebuffer, &fb, sizeof(state->framebuffer) != 0)))
    {
        if (fb.handle != RENDER_FRAMEBUFFER_INVALID)
        {
            glEnable(GL_TEXTURE_2D);
            glBindFramebuffer(GL_FRAMEBUFFER, fb.handle);
            glBindTexture(GL_TEXTURE_2D, fb.texture);
        }
        else
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        state->framebuffer = fb;

        return true;
    }
    // return false;
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
    // FIXME epatitucci
    // if (forced || (state->viewport.left != left || state->viewport.right != right ||
    //                state->viewport.bottom != bottom || state->viewport.top != top ||
    //                state->viewport.nearZ != nearZ || state->viewport.farZ != farZ ||
    //                state->viewport.scale != state->viewport.scale))
    {
        ScopedPerfMarker(__FUNCTION__);

        int right = scene_x + scene_width;
        // game viewport isn't scaled, if the OS window is smaller than GameWindowPosY + GameWindowHeight, bottom will
        // be negative by this difference
        int needed_height = scene_y + scene_height;
        int bottom = window_height - needed_height;

        state->viewport.left = scene_x;
        state->viewport.right = right;
        state->viewport.bottom = bottom;
        state->viewport.top = scene_y;
        state->viewport.nearZ = camera_nearZ;
        state->viewport.farZ = camera_farZ;
        state->viewport.scale = scene_scale;
        state->viewport.proj_flipped_y = proj_flipped_y;

        glViewport(scene_x, bottom, scene_width, scene_height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        float scaledRight = right * scene_scale;
        float scaledLeft = scene_x * scene_scale - (scaledRight - right);
        float scaledBottom = (scene_y + scene_height) * scene_scale;
        float scaledTop = scene_y * scene_scale - (scaledBottom - (scene_y + scene_height));

        if (!proj_flipped_y)
        {
            glOrtho(
                scaledLeft,
                scaledRight,
                scaledBottom,
                scaledTop,
                float(camera_nearZ),
                float(camera_farZ));
        }
        else
        {
            glOrtho(
                scaledLeft,
                scaledRight,
                scaledTop,
                scaledBottom,
                float(camera_nearZ),
                float(camera_farZ));
        }
        glMatrixMode(GL_MODELVIEW);
        return true;
    }

    // return false;
}

bool RenderState_SetModelViewTranslation(RenderState *state, float3 pos, bool forced)
{
    glTranslatef(pos[0], pos[1], pos[2]);

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
            glEnable(GL_SCISSOR_TEST);
        }
        else
        {
            glDisable(GL_SCISSOR_TEST);
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

        glScissor(x, y, width, height);
    }
    return changed;
}
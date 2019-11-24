#include "Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "Renderer/RenderInternal.h"

bool RenderState_FlushState(RenderState *state)
{
    RenderState_SetTexture(state, state->texture.type, state->texture.texture, true);
    RenderState_SetBlend(state, state->blend.enabled, state->blend.func, true);
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

    static_assert(countof(s_alphaTestfuncToOGLFunc) == AlphaTestFunc::AlphaTest_Count);

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

    if (enabled && differentFuncOrRef() || (forced && func != AlphaTestFunc::AlphaTest_Invalid))
    {
        changed = true;
        state->alphaTest.func = func;
        state->alphaTest.alphaRef = ref;
        glAlphaFunc(s_alphaTestfuncToOGLFunc[func], ref);
    }

    return changed;
}

bool RenderState_SetBlend(RenderState *state, bool enabled, BlendFunc func, bool forced)
{
    static GLenum s_blendSrcComponentToOGLEnum[] = {
        GL_SRC_ALPHA, // SrcAlpha_OneMinusSrcAlpha
        GL_ONE,       // One_OneMinusSrcAlpha
        GL_DST_COLOR, // DstColor_Zero
        GL_ZERO,      // Zero_OneMinusSrcAlpha
        GL_ZERO       // Zero_SrcColor
    };

    static GLenum s_blendDstComponentToOGLEnum[] = {
        GL_ONE_MINUS_SRC_ALPHA, // SrcAlpha_OneMinusSrcAlpha
        GL_ONE_MINUS_SRC_ALPHA, // One_OneMinusSrcAlpha
        GL_ZERO,                // DstColor_Zero
        GL_ONE_MINUS_SRC_ALPHA, // Zero_OneMinusSrcAlpha
        GL_SRC_COLOR            // Zero_SrcColor
    };

    static_assert(countof(s_blendSrcComponentToOGLEnum) == BlendFunc::BlendFunc_Count);
    static_assert(countof(s_blendSrcComponentToOGLEnum) == countof(s_blendDstComponentToOGLEnum));

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

    if (enabled && state->blend.func != func || (forced && func != BlendFunc::BlendFunc_Invalid))
    {
        changed = true;
        state->blend.func = func;
        glBlendFunc(s_blendSrcComponentToOGLEnum[func], s_blendDstComponentToOGLEnum[func]);
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

    static_assert(countof(s_depthFuncToOGLFunc) == DepthFunc::DepthFunc_Count);

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
        GL_KEEP,      // StencilOp::Keep
        GL_ZERO,      // StencilOp::Zero
        GL_REPLACE,   // StencilOp::Replace
        GL_INCR,      // StencilOp::IncrementClamp
        GL_INCR_WRAP, // StencilOp::IncrementWrap
        GL_DECR,      // StencilOp::DecrementClamp,
        GL_DECR_WRAP, // StencilOp::DecrementWrap,
        GL_INVERT     // StencilOp::Invert
    };

    static_assert(countof(s_stencilFuncToOGLFunc) == StencilFunc::StencilFunc_Count);
    static_assert(countof(s_stencilOpToOGLOp) == StencilOp::StencilOp_Count);

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
            mask & ColorMask::Red ? GL_TRUE : GL_FALSE,
            mask & ColorMask::Green ? GL_TRUE : GL_FALSE,
            mask & ColorMask::Blue ? GL_TRUE : GL_FALSE,
            mask & ColorMask::Alpha ? GL_TRUE : GL_FALSE);
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
    RenderState *state, uint32_t id, void *value, ShaderUniformType type, bool forced)
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

    if (!state->uniformCache.dataSet[id] ||
        memcmp(state->uniformCache.data[id], value, uniformSize) != 0)
    {
        switch (type)
        {
            case ShaderUniformType::Int1:
            {
                auto typedValue = *(GLint *)value;
                glUniform1i(location, typedValue);
                break;
            }

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
    }

    return true;
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
        case ShaderUniformType::Float1V:
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

    return false;
}

bool RenderState_SetTexture(
    RenderState *state, RenderTextureType type, texture_handle_t texture, bool forced)
{
    auto textureTypeToOGLType = [](RenderTextureType type) {
        switch (type)
        {
            case RenderTextureType::Texture2D:
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
        }

        return true;
    }

    return false;
}
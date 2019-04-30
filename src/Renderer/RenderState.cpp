#include "Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "Renderer/RenderInternal.h"

bool RenderState_FlushState(RenderState *state)
{
    RenderState_SetTexture(state, state->texture.type, state->texture.texture, true);
    RenderState_SetBlend(state, state->blend.enabled, state->blend.func, true);
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
    RenderState_SetColorMask(state, state->colorMask);
    // RenderState_SetShaderPipeline(state, &state->pipeline, true);
    // FIXME uniform cache is not applied during flush, not sure if it should be applied or if the behavior
    // should be clear
    // TODO add a compile-time assert to ensure any newly added command is applied or properly ignored here

    return true;
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
        GL_NEVER,    // StencilFunc::NeverPass
        GL_LESS,     // StencilFunc::Less
        GL_LEQUAL,   // StencilFunc::LessOrEqual
        GL_GREATER,  // StencilFunc::Greater
        GL_GEQUAL,   // StencilFunc::GreaterOrEqual
        GL_EQUAL,    // StencilFunc::Equal
        GL_NOTEQUAL, // StencilFunc::Different
        GL_ALWAYS    // StencilFunc::AlwaysPass
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
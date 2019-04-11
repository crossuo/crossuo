#include "Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "Renderer/RenderInternal.h"

bool RenderState_FlushState(RenderState *state)
{
    RenderState_SetTexture(state, state->texture.type, state->texture.texture, true);
    RenderState_SetBlend(state, state->blend.enabled, state->blend.func, true);
    RenderState_SetStencil(state, state->stencil.enabled, true);

    return true;
}

bool RenderState_SetBlend(RenderState *state, bool enabled, BlendFunc func, bool forced)
{
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

    if (func != BlendFunc::BlendFunc_Invalid && state->blend.func != func || forced)
    {
        changed = true;
        state->blend.func = func;
        switch (func)
        {
            case BlendFunc::SrcAlpha_OneMinusSrcAlpha:
            {
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                break;
            }

            default:
            {
                assert(false);
                break;
            }
        }
    }

    return changed;
}

bool RenderState_SetStencil(RenderState *state, bool enabled, bool forced)
{
    if (state->stencil.enabled != enabled || forced)
    {
        state->stencil.enabled = enabled;
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

    auto uniformCacheOffset = state->uniformCache.uniforms[id].offset;
    auto uniformSize = Render_ShaderUniformTypeToSize(type);

    if (uniformCacheOffset == 0xffffffff ||
        memcmp(&state->uniformCache.data[uniformCacheOffset], value, uniformSize) != 0)
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

        if (uniformSize + state->uniformCache.dataUsedSize > RENDERSTATE_SHADER_UNIFORMDATA_SIZE)
        {
            Warning(
                Renderer,
                "Not enough space to store shader uniform (%d bytes) in the uniform cache, increase RENDERSTATE_SHADER_UNIFORMDATA_SIZE (%d bytes)",
                uniformSize,
                RENDERSTATE_SHADER_UNIFORMDATA_SIZE);
        }
        else
        {
            // state->uniformCache.uniforms[id].type = type;
            state->uniformCache.uniforms[id].offset = state->uniformCache.dataUsedSize;
            memcpy(&state->uniformCache.data[state->uniformCache.dataUsedSize], value, uniformSize);
            state->uniformCache.dataUsedSize += uniformSize;
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
    if (state->pipeline.program == RENDER_SHADERPROGRAM_INVALID)
    {
        return false;
    }

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
    if (state->pipeline.program != RENDER_SHADERPROGRAM_INVALID || forced)
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
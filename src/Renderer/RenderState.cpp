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

    if (func != BlendFunc::Invalid && state->blend.func != func || forced)
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

bool RenderState_SetTexture(
    RenderState *state, RenderTextureType type, textureHandle_t texture, bool forced)
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
        if (texture == TEXTUREHANDLE_INVALID)
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
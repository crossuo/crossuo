#include "Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "Renderer/RenderInternal.h"

#define MATCH_CASE_DRAW_DEBUG(type, cmd, state)                                                    \
    case RenderCommandType::Cmd_##type:                                                            \
    {                                                                                              \
        RenderDraw_##type##Debug((type##Cmd *)cmd, state);                                         \
        (char *)cmd += sizeof(type##Cmd);                                                          \
        break;                                                                                     \
    }

static const char *BlendFuncAsString(BlendFunc func)
{
    static const char *s_funcToText[] = { "SrcAlpha_OneMinusSrcAlpha", "One_OneMinusSrcAlpha" };
    static_assert(countof(s_funcToText) == BlendFunc::BlendFunc_Count);

    if (func == BlendFunc::BlendFunc_Invalid)
    {
        return "Invalid";
    }

    assert(func < BlendFunc::BlendFunc_Count);
    return s_funcToText[func];
}

const char *ShaderUniformTypeAsString(ShaderUniformType type)
{
    static const char *s_uniformTypeAsStr[] = {
        "Int1",    // ShaderUniformType::Int1
        "Float1V", // ShaderUniformType::Float1V
    };
    static_assert(countof(s_uniformTypeAsStr) == ShaderUniformType::ShaderUniform_Count);

    return s_uniformTypeAsStr[type];
}

void ShaderUniformValueAsString(
    ShaderUniformType type, void *value, uint32_t count, char *str, size_t str_capacity)
{
    if (type >= ShaderUniformType::ShaderUniform_FixedFirst &&
        type <= ShaderUniformType::ShaderUniform_FixedLast)
    {
        switch (type)
        {
            case ShaderUniformType::Int1:
                snprintf(str, str_capacity, "%d", *(GLint *)value);
                break;

            default:
                assert(false);
                break;
        }
    }
    else
    {
        assert(
            type >= ShaderUniformType::ShaderUniform_VariableFirst &&
            type <= ShaderUniformType::ShaderUniform_VariableLast);
        switch (type)
        {
            case ShaderUniformType::Float1V:
            {
                snprintf(str, str_capacity, "%f, ...", *(GLfloat *)value);
                break;
            }

            default:
                assert(false);
                break;
        }
    }
}

static const char *TextureTypeAsString(RenderTextureType type)
{
    static const char *s_textureTypeAsStr[] = {
        "Texture2D" // Texture2D
    };

    static_assert(countof(s_textureTypeAsStr) == RenderTextureType::RenderTextureType_Count);

    assert(type < RenderTextureType::RenderTextureType_Count);
    return s_textureTypeAsStr[type];
}

void RenderDraw_DrawQuadDebug(DrawQuadCmd *cmd, RenderState *state)
{
    Info(
        Renderer,
        "DrawQuadCmd: texture: %d - x: %d - y: %d - width: %d - height: %d - u: %f - v: %f - rgba: [%f, %f, %f, %f] - mirrored: %s\n",
        cmd->texture,
        cmd->x,
        cmd->y,
        cmd->width,
        cmd->height,
        cmd->u,
        cmd->v,
        cmd->rgba[0],
        cmd->rgba[1],
        cmd->rgba[2],
        cmd->rgba[3],
        cmd->mirrored ? "true" : "false");
}

void RenderDraw_DrawRotatedQuadDebug(DrawRotatedQuadCmd *cmd, RenderState *state)
{
    Info(
        Renderer,
        "RotatedTextureCmd: texture: %d - x: %d - y: %d - width: %d - height: %d - angle - %f - u: %f - v: %f - rgba: [%f, %f, %f, %f] - mirrored: %s\n",
        cmd->texture,
        cmd->x,
        cmd->y,
        cmd->width,
        cmd->height,
        cmd->angle,
        cmd->u,
        cmd->v,
        cmd->rgba[0],
        cmd->rgba[1],
        cmd->rgba[2],
        cmd->rgba[3],
        cmd->mirrored ? "true" : "false");
}

void RenderDraw_FlushStateDebug(FlushStateCmd *, RenderState *state)
{
    Info(Renderer, "FlushStateCmd\n");
}

void RenderDraw_SetTextureDebug(SetTextureCmd *cmd, RenderState *state)
{
    auto typeAsStr = TextureTypeAsString(cmd->type);
    Info(Renderer, "SetTextureCmd: texture: %d - type: %s\n", cmd->texture, typeAsStr);
}

void RenderDraw_BlendStateDebug(BlendStateCmd *cmd, RenderState *state)
{
    Info(Renderer, "BlendStateCmd: func: %s\n", BlendFuncAsString(cmd->func));
}

void RenderDraw_DisableBlendStateDebug(DisableBlendStateCmd *cmd, RenderState *state)
{
    Info(Renderer, "DisableBlendStateCmd\n");
}

void RenderDraw_StencilStateDebug(StencilStateCmd *cmd, RenderState *state)
{
    Info(Renderer, "StencilStateCmd\n");
}

void RenderDraw_DisableStencilStateDebug(DisableStencilStateCmd *cmd, RenderState *state)
{
    Info(Renderer, "DisableStencilStateCmd\n");
}

void RenderDraw_ShaderUniformDebug(ShaderUniformCmd *cmd, RenderState *state)
{
    assert(
        cmd->type >= ShaderUniformType::ShaderUniform_FixedFirst &&
        cmd->type <= ShaderUniformType::ShaderUniform_FixedLast);

    auto typeAsStr = ShaderUniformTypeAsString(cmd->type);
    char valueAsStr[64] = "<invalid>";
    ShaderUniformValueAsString(cmd->type, &cmd->value[0], 1, valueAsStr, countof(valueAsStr));

    auto pipelineValid = state->pipeline.program != RENDER_SHADERPROGRAM_INVALID;
    assert((pipelineValid && cmd->id < countof(state->pipeline.uniforms)) || !pipelineValid);
    Info(
        Renderer,
        "ShaderUniformCmd: id: %d - location: %d - value: %s - type: %s\n",
        pipelineValid ? cmd->id : RENDER_SHADERUNIFORMID_INVALID,
        pipelineValid ? state->pipeline.uniforms[cmd->id].location :
                        RENDER_SHADERUNIFORMLOC_INVALID,
        valueAsStr,
        typeAsStr);
}

void RenderDraw_ShaderLargeUniformDebug(ShaderLargeUniformCmd *cmd, RenderState *state)
{
    assert(
        cmd->type >= ShaderUniformType::ShaderUniform_VariableFirst &&
        cmd->type <= ShaderUniformType::ShaderUniform_VariableLast);

    auto typeAsStr = ShaderUniformTypeAsString(cmd->type);
    char valueAsStr[64] = "<invalid>";
    ShaderUniformValueAsString(cmd->type, cmd->value, cmd->count, valueAsStr, countof(valueAsStr));

    auto pipelineValid = state->pipeline.program != RENDER_SHADERPROGRAM_INVALID;
    assert((pipelineValid && cmd->id < countof(state->pipeline.uniforms)) || !pipelineValid);
    Info(
        Renderer,
        "ShaderLargeUniformCmd: id: %d - location: %d - count: %d - values: %s - type: %s\n",
        pipelineValid ? cmd->id : RENDER_SHADERUNIFORMID_INVALID,
        pipelineValid ? state->pipeline.uniforms[cmd->id].location :
                        RENDER_SHADERUNIFORMLOC_INVALID,
        cmd->count,
        valueAsStr,
        typeAsStr);
}

void RenderDraw_ShaderPipelineDebug(ShaderPipelineCmd *cmd, RenderState *state)
{
    // TODO implement RenderDraw_ShaderPipelineDebug
    assert(false);
}

void RenderDraw_DisableShaderPipelineDebug(DisableShaderPipelineCmd *cmd, RenderState *state)
{
    // TODO implement RenderDraw_DisableShaderPipelineDebug
    assert(false);
}

void RenderDraw_DumpCmdList(RenderCmdList *cmdList)
{
    Info(
        Renderer,
        "Dumping cmd list %p, data %p, capacity %dkB, free size %dkB, immediate? %s",
        cmdList,
        cmdList->data,
        cmdList->size,
        cmdList->remainingSize,
        cmdList->immediateMode);

    char *cmd = cmdList->data;
    uint32_t remainingCmdSize = cmdList->size - cmdList->remainingSize;
    char *listEnd = cmd + remainingCmdSize;

    while (cmd < listEnd)
    {
        RenderCommandHeader &cmdHeader = *(RenderCommandHeader *)cmd;
        switch (cmdHeader.type)
        {
            MATCH_CASE_DRAW_DEBUG(DrawQuad, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DrawRotatedQuad, cmd, &cmdList->state)

            MATCH_CASE_DRAW_DEBUG(FlushState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(SetTexture, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(BlendState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DisableBlendState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(StencilState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DisableStencilState, cmd, &cmdList->state)

            MATCH_CASE_DRAW_DEBUG(ShaderUniform, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(ShaderLargeUniform, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(ShaderPipeline, cmd, &cmdList->state);
            MATCH_CASE_DRAW_DEBUG(DisableShaderPipeline, cmd, &cmdList->state);

                // TODO SetColorMask
                // TODO ClearRT
                // TODO DrawCharacterSitting

            default:
                assert(false);
                break;
        }
    }
}
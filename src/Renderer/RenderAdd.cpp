#include "Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "Renderer/RenderInternal.h"

TextureCmd RenderAdd_TextureCmd(
    texture_handle_t texture,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    float u,
    float v,
    float4 rgba)
{
    return TextureCmd(texture, x, y, width, height, u, v, rgba);
}

RotatedTextureCmd RenderAdd_RotatedTextureCmd(
    texture_handle_t texture,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    float angle,
    float u,
    float v,
    float4 rgba)
{
    return RotatedTextureCmd(texture, x, y, width, height, angle, u, v, rgba);
}

bool RenderAdd_Texture(RenderCmdList *cmdList, TextureCmd *textures, uint32_t texture_count)
{
    auto ret = Render_AppendCmd(cmdList, textures, texture_count * sizeof(TextureCmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    // FIXME RenderDraw_Texture(textures, texture_count, cmdList->state);
    ret = true;
    for (auto i = 0u; i < texture_count; i++)
    {
        ret &= RenderDraw_Texture(textures + i, &cmdList->state);
    }

    return ret;
}

bool RenderAdd_RotatedTexture(
    RenderCmdList *cmdList, RotatedTextureCmd *textures, uint32_t texture_count)
{
    auto ret = Render_AppendCmd(cmdList, textures, texture_count * sizeof(RotatedTextureCmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    // FIXME RenderDraw_Texture(textures, texture_count, cmdList->state);
    ret = true;
    for (auto i = 0u; i < texture_count; i++)
    {
        ret &= RenderDraw_RotatedTexture(textures + i, &cmdList->state);
    }

    return ret;
}

BlendStateCmd RenderAdd_Blend(BlendFunc func)
{
    return BlendStateCmd(func);
}

bool RenderAdd_SetBlend(RenderCmdList *cmdList, BlendStateCmd *cmd)
{
    auto ret = Render_AppendCmd(cmdList, cmd, sizeof(BlendStateCmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    return RenderDraw_BlendState(cmd, &cmdList->state);
}

bool RenderAdd_DisableBlend(RenderCmdList *cmdList)
{
    DisableBlendStateCmd cmd;
    auto ret = Render_AppendCmd(cmdList, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    return RenderDraw_DisableBlendState(&cmd, &cmdList->state);
}

StencilStateCmd RenderAdd_Stencil()
{
    return StencilStateCmd();
}

ShaderUniformCmd RenderAdd_ShaderUniformCmd(uint32_t id, void *value, ShaderUniformType type)
{
    return ShaderUniformCmd(id, value, type);
}

ShaderLargeUniformCmd
RenderAdd_ShaderLargeUniformCmd(uint32_t id, void *value, uint32_t count, ShaderUniformType type)
{
    return ShaderLargeUniformCmd(id, value, count, type);
}

bool RenderAdd_SetStencil(RenderCmdList *cmdList, StencilStateCmd *state)
{
    auto ret = Render_AppendCmd(cmdList, state, sizeof(StencilStateCmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    return RenderDraw_StencilState(state, &cmdList->state);
}

bool RenderAdd_DisableStencil(RenderCmdList *cmdList)
{
    DisableStencilStateCmd cmd;
    auto ret = Render_AppendCmd(cmdList, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    return RenderDraw_DisableStencilState(&cmd, &cmdList->state);
}

bool RenderAdd_SetShaderUniform(RenderCmdList *cmdList, ShaderUniformCmd *cmd)
{
    auto ret = Render_AppendCmd(cmdList, cmd, sizeof(ShaderUniformCmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    return RenderDraw_ShaderUniform(cmd, &cmdList->state);
}

bool RenderAdd_SetShaderLargeUniform(RenderCmdList *cmdList, ShaderLargeUniformCmd *cmd)
{
    auto ret = Render_AppendCmd(cmdList, cmd, sizeof(ShaderLargeUniformCmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    return RenderDraw_ShaderLargeUniform(cmd, &cmdList->state);
}

bool RenderAdd_SetShaderPipeline(RenderCmdList *cmdList, ShaderPipeline *pipeline)
{
    ShaderPipelineCmd cmd{ pipeline };
    auto ret = Render_AppendCmd(cmdList, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    return RenderDraw_ShaderPipeline(&cmd, &cmdList->state);
}

bool RenderAdd_DisableShaderPipeline(RenderCmdList *cmdList)
{
    DisableShaderPipelineCmd cmd{};
    auto ret = Render_AppendCmd(cmdList, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    return RenderDraw_DisableShaderPipeline(&cmd, &cmdList->state);
}

bool RenderAdd_FlushState(RenderCmdList *cmdList)
{
    FlushStateCmd cmd;
    auto ret = Render_AppendCmd(cmdList, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    return RenderDraw_FlushState(&cmd, &cmdList->state);
}

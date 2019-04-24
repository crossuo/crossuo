#include "Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "Renderer/RenderInternal.h"

bool RenderAdd_SetTexture(RenderCmdList *cmdList, SetTextureCmd *cmd)
{
    auto ret = Render_AppendCmd(cmdList, cmd, sizeof(*cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_SetTexture(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_DrawQuad(RenderCmdList *cmdList, DrawQuadCmd *cmds, uint32_t cmd_count)
{
    auto ret = Render_AppendCmd(cmdList, cmds, cmd_count * sizeof(*cmds));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    // FIXME RenderDraw_DrawQuad(textures, texture_count, cmdList->state);
    for (auto i = 0u; i < cmd_count; i++)
    {
        RenderDraw_DrawQuad(cmds + i, &cmdList->state);
    }

    return true;
}

bool RenderAdd_DrawRotatedQuad(RenderCmdList *cmdList, DrawRotatedQuadCmd *cmds, uint32_t cmd_count)
{
    auto ret = Render_AppendCmd(cmdList, cmds, cmd_count * sizeof(*cmds));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    // FIXME RenderDraw_DrawQuad(textures, texture_count, cmdList->state);
    for (auto i = 0u; i < cmd_count; i++)
    {
        RenderDraw_DrawRotatedQuad(cmds + i, &cmdList->state);
    }

    return true;
}

bool RenderAdd_DrawCharacterSitting(RenderCmdList *cmdList, DrawCharacterSittingCmd *cmd)
{
    auto ret = Render_AppendCmd(cmdList, cmd, sizeof(*cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DrawCharacterSitting(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_DrawLandTile(RenderCmdList *cmdList, DrawLandTileCmd *cmd)
{
    auto ret = Render_AppendCmd(cmdList, cmd, sizeof(*cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DrawLandTile(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetBlend(RenderCmdList *cmdList, BlendStateCmd *cmd)
{
    auto ret = Render_AppendCmd(cmdList, cmd, sizeof(BlendStateCmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_BlendState(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_DisableBlend(RenderCmdList *cmdList)
{
    DisableBlendStateCmd cmd;
    auto ret = Render_AppendCmd(cmdList, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DisableBlendState(&cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetStencil(RenderCmdList *cmdList, StencilStateCmd *state)
{
    auto ret = Render_AppendCmd(cmdList, state, sizeof(StencilStateCmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_StencilState(state, &cmdList->state);
    return true;
}

bool RenderAdd_DisableStencil(RenderCmdList *cmdList)
{
    DisableStencilStateCmd cmd;
    auto ret = Render_AppendCmd(cmdList, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DisableStencilState(&cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetColorMask(RenderCmdList *cmdList, SetColorMaskCmd *cmd)
{
    auto ret = Render_AppendCmd(cmdList, cmd, sizeof(*cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_SetColorMask(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_ClearRT(RenderCmdList *cmdList, ClearRTCmd *cmd)
{
    auto ret = Render_AppendCmd(cmdList, cmd, sizeof(*cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_ClearRT(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetShaderUniform(RenderCmdList *cmdList, ShaderUniformCmd *cmd)
{
    auto ret = Render_AppendCmd(cmdList, cmd, sizeof(ShaderUniformCmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_ShaderUniform(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetShaderLargeUniform(RenderCmdList *cmdList, ShaderLargeUniformCmd *cmd)
{
    auto ret = Render_AppendCmd(cmdList, cmd, sizeof(ShaderLargeUniformCmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_ShaderLargeUniform(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetShaderPipeline(RenderCmdList *cmdList, ShaderPipeline *pipeline)
{
    ShaderPipelineCmd cmd{ pipeline };
    auto ret = Render_AppendCmd(cmdList, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_ShaderPipeline(&cmd, &cmdList->state);
    return true;
}

bool RenderAdd_DisableShaderPipeline(RenderCmdList *cmdList)
{
    DisableShaderPipelineCmd cmd{};
    auto ret = Render_AppendCmd(cmdList, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DisableShaderPipeline(&cmd, &cmdList->state);
    return true;
}

bool RenderAdd_FlushState(RenderCmdList *cmdList)
{
    FlushStateCmd cmd;
    auto ret = Render_AppendCmd(cmdList, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_FlushState(&cmd, &cmdList->state);
    return true;
}

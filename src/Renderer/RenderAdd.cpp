#include "Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "Renderer/RenderInternal.h"

bool RenderAdd_SetTexture(RenderCmdList *cmdList, const SetTextureCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_SetTexture(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetFrameBuffer(RenderCmdList *cmdList, const SetFrameBufferCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_SetFrameBuffer(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_DrawQuad(RenderCmdList *cmdList, const DrawQuadCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DrawQuad(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_DrawRotatedQuad(RenderCmdList *cmdList, const DrawRotatedQuadCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DrawRotatedQuad(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_DrawCharacterSitting(RenderCmdList *cmdList, const DrawCharacterSittingCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DrawCharacterSitting(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_DrawLandTile(RenderCmdList *cmdList, const DrawLandTileCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DrawLandTile(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_DrawShadow(RenderCmdList *cmdList, const DrawShadowCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DrawShadow(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_DrawCircle(RenderCmdList *cmdList, const DrawCircleCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DrawCircle(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_DrawUntexturedQuad(RenderCmdList *cmdList, const DrawUntexturedQuadCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DrawUntexturedQuad(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_DrawLine(RenderCmdList *cmdList, const DrawLineCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DrawLine(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetAlphaTest(RenderCmdList *cmdList, const AlphaTestCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_AlphaTest(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_DisableAlphaTest(RenderCmdList *cmdList)
{
    DisableAlphaTestCmd cmd;
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DisableAlphaTest(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetBlend(RenderCmdList *cmdList, const BlendStateCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
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
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DisableBlendState(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetStencil(RenderCmdList *cmdList, const StencilStateCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_StencilState(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_DisableStencil(RenderCmdList *cmdList)
{
    DisableStencilStateCmd cmd;
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DisableStencilState(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_EnableStencil(RenderCmdList *cmdList)
{
    EnableStencilStateCmd cmd;
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_EnableStencilState(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetDepth(RenderCmdList *cmdList, const DepthStateCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DepthState(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_DisableDepth(RenderCmdList *cmdList)
{
    DisableDepthStateCmd cmd;
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DisableDepthState(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_EnableDepth(RenderCmdList *cmdList)
{
    EnableDepthStateCmd cmd;
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_EnableDepthState(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetColorMask(RenderCmdList *cmdList, const SetColorMaskCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_SetColorMask(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetColor(RenderCmdList *cmdList, const SetColorCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_SetColor(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetClearColor(RenderCmdList *cmdList, const SetClearColorCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_SetClearColor(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_ClearRT(RenderCmdList *cmdList, const ClearRTCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_ClearRT(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetShaderUniform(RenderCmdList *cmdList, const ShaderUniformCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_ShaderUniform(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetShaderLargeUniform(RenderCmdList *cmdList, const ShaderLargeUniformCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_ShaderLargeUniform(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetShaderPipeline(RenderCmdList *cmdList, ShaderPipeline *pipeline)
{
    // commenting this out to avoid vs2015 bug
    // ShaderPipelineCmd cmd{ pipeline };
    ShaderPipelineCmd cmd;
    cmd.pipeline = pipeline;
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_ShaderPipeline(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_DisableShaderPipeline(RenderCmdList *cmdList)
{
    DisableShaderPipelineCmd cmd{};
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DisableShaderPipeline(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_FlushState(RenderCmdList *cmdList)
{
    FlushStateCmd cmd;
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_FlushState(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetViewParams(RenderCmdList *cmdList, const SetViewParamsCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_SetViewParams(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetModelViewTranslation(
    RenderCmdList *cmdList, const SetModelViewTranslationCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_SetModelViewTranslation(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_SetScissor(RenderCmdList *cmdList, const SetScissorCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_SetScissor(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_DisableScissor(RenderCmdList *cmdList)
{
    DisableScissorCmd cmd;
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_DisableScissor(cmd, &cmdList->state);
    return true;
}

bool RenderAdd_GetFrameBufferPixels(RenderCmdList *cmdList, const GetFrameBufferPixelsCmd &cmd)
{
    auto ret = Render_AppendCmdType(cmdList, cmd._type, &cmd, sizeof(cmd));
    if (!cmdList->immediateMode)
    {
        return ret;
    }

    RenderDraw_GetFrameBufferPixels(cmd, &cmdList->state);
    return true;
}
#pragma once

#ifndef RENDERER_INTERNAL
#error "Do not include this directly. Include RenderAPI.h"
#endif

uint32_t Render_ShaderUniformTypeToSize(ShaderUniformType type);

bool Render_AppendCmd(RenderCmdList *cmdList, void *cmd, uint32_t cmdSize);
bool RenderState_SetTexture(
    RenderState *state, RenderTextureType type, texture_handle_t texture, bool forced = false);
bool RenderState_FlushState(RenderState *state);
bool RenderState_SetBlend(RenderState *state, bool enabled, BlendFunc func, bool forced = false);
bool RenderState_SetStencil(RenderState *state, bool enabled, bool forced = false);
bool RenderState_SetShaderUniform(
    RenderState *state, uint32_t slot, void *value, ShaderUniformType type, bool forced = false);
bool RenderState_SetShaderLargeUniform(
    RenderState *state,
    uint32_t slot,
    void *value,
    uint32_t count,
    ShaderUniformType type,
    bool forced = false);
bool RenderState_SetShaderPipeline(
    RenderState *state, ShaderPipeline *pipeline, bool forced = false);
bool RenderState_DisableShaderPipeline(RenderState *state, bool forced = false);

bool RenderDraw_Texture(TextureCmd *cmd, RenderState *state);
bool RenderDraw_RotatedTexture(RotatedTextureCmd *cmd, RenderState *state);
bool RenderDraw_BlendState(BlendStateCmd *cmd, RenderState *state);
bool RenderDraw_DisableBlendState(DisableBlendStateCmd *, RenderState *state);
bool RenderDraw_FlushState(FlushStateCmd *cmd, RenderState *state);
bool RenderDraw_StencilState(StencilStateCmd *, RenderState *state);
bool RenderDraw_DisableStencilState(DisableStencilStateCmd *, RenderState *state);
bool RenderDraw_ShaderUniform(ShaderUniformCmd *, RenderState *state);
bool RenderDraw_ShaderLargeUniform(ShaderLargeUniformCmd *, RenderState *state);
bool RenderDraw_ShaderPipeline(ShaderPipelineCmd *cmd, RenderState *state);
bool RenderDraw_DisableShaderPipeline(DisableShaderPipelineCmd *cmd, RenderState *state);
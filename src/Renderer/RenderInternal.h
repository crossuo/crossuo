// GPLv3 License
// Copyright (C) 2019 Everton Fernando Patitucci da Silva

#pragma once

#ifndef RENDERER_INTERNAL
#error "Do not include this directly. Include RenderAPI.h"
#endif

#define OGL_DEBUGCONTEXT_ENABLED

uint32_t Render_ShaderUniformTypeToSize(ShaderUniformType type);
// FIXME naming
void SetupOGLDebugMessage();

bool Render_AppendCmd(RenderCmdList *cmdList, void *cmd, uint32_t cmdSize);

bool RenderState_SetTexture(
    RenderState *state, TextureType type, texture_handle_t texture, bool forced = false);
bool RenderState_SetFrameBuffer(RenderState *state, frame_buffer_t fb, bool forced = false);

bool RenderState_FlushState(RenderState *state);
bool RenderState_SetAlphaTest(
    RenderState *state, bool enabled, AlphaTestFunc func, float alphaRef, bool forced = false);
bool RenderState_SetBlend(
    RenderState *state,
    bool enabled,
    BlendFactor src,
    BlendFactor dst,
    BlendEquation equation,
    bool forced = false);
bool RenderState_SetDepth(RenderState *state, bool enabled, DepthFunc func, bool forced = false);
bool RenderState_SetDepthEnabled(RenderState *state, bool enabled, bool forced = false);
bool RenderState_SetStencil(
    RenderState *state,
    bool enabled,
    StencilFunc func,
    uint32_t ref,
    uint32_t mask,
    StencilOp stencilFail,
    StencilOp depthFail,
    StencilOp bothFail,
    bool forced = false);
bool RenderState_SetStencilEnabled(RenderState *state, bool enabled, bool forced = false);
bool RenderState_SetColorMask(RenderState *state, ColorMask mask, bool forced = false);
bool RenderState_SetColor(RenderState *state, float4 color, bool forced = false);
bool RenderState_SetClearColor(RenderState *state, float4 color, bool forced = false);
bool RenderState_ClearRT(RenderState *state, ClearRT mask, bool forced = false);
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
    bool forced = false);
bool RenderState_SetModelViewTranslation(RenderState *state, float3 pos, bool forced = false);
bool RenderState_SetScissor(
    RenderState *state,
    bool enabled,
    int x,
    int y,
    uint32_t width,
    uint32_t height,
    bool forced = false);

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

bool RenderDraw_SetTexture(SetTextureCmd *cmd, RenderState *state);
bool RenderDraw_SetFrameBuffer(SetFrameBufferCmd *cmd, RenderState *state);
bool RenderDraw_DrawQuad(DrawQuadCmd *cmd, RenderState *state);
bool RenderDraw_DrawRotatedQuad(DrawRotatedQuadCmd *cmd, RenderState *state);
bool RenderDraw_DrawCharacterSitting(DrawCharacterSittingCmd *cmd, RenderState *state);
bool RenderDraw_DrawLandTile(DrawLandTileCmd *cmd, RenderState *state);
bool RenderDraw_DrawShadow(DrawShadowCmd *cmd, RenderState *state);
bool RenderDraw_DrawCircle(DrawCircleCmd *cmd, RenderState *state);
bool RenderDraw_DrawUntexturedQuad(DrawUntexturedQuadCmd *cmd, RenderState *state);
bool RenderDraw_DrawLine(DrawLineCmd *cmd, RenderState *state);

bool RenderDraw_FlushState(FlushStateCmd *cmd, RenderState *state);
bool RenderDraw_SetViewParams(SetViewParamsCmd *cmd, RenderState *state);
bool RenderDraw_SetModelViewTranslation(SetModelViewTranslationCmd *cmd, RenderState *state);
bool RenderDraw_SetScissor(SetScissorCmd *cmd, RenderState *state);
bool RenderDraw_DisableScissor(DisableScissorCmd *cmd, RenderState *state);
bool RenderDraw_AlphaTest(AlphaTestCmd *cmd, RenderState *state);
bool RenderDraw_DisableAlphaTest(DisableAlphaTestCmd *, RenderState *state);
bool RenderDraw_BlendState(BlendStateCmd *cmd, RenderState *state);
bool RenderDraw_DisableBlendState(DisableBlendStateCmd *, RenderState *state);
bool RenderDraw_StencilState(StencilStateCmd *cmd, RenderState *state);
bool RenderDraw_DisableStencilState(DisableStencilStateCmd *, RenderState *state);
bool RenderDraw_EnableStencilState(EnableStencilStateCmd *, RenderState *state);
bool RenderDraw_DepthState(DepthStateCmd *depth, RenderState *state);
bool RenderDraw_DisableDepthState(DisableDepthStateCmd *, RenderState *state);
bool RenderDraw_EnableDepthState(EnableDepthStateCmd *, RenderState *state);
bool RenderDraw_SetColorMask(SetColorMaskCmd *cmd, RenderState *state);
bool RenderDraw_SetColor(SetColorCmd *cmd, RenderState *state);
bool RenderDraw_SetClearColor(SetClearColorCmd *cmd, RenderState *state);
bool RenderDraw_ClearRT(ClearRTCmd *cmd, RenderState *state);

bool RenderDraw_ShaderUniform(ShaderUniformCmd *, RenderState *state);
bool RenderDraw_ShaderLargeUniform(ShaderLargeUniformCmd *, RenderState *state);
bool RenderDraw_ShaderPipeline(ShaderPipelineCmd *cmd, RenderState *state);
bool RenderDraw_DisableShaderPipeline(DisableShaderPipelineCmd *cmd, RenderState *state);

bool RenderDraw_GetFrameBufferPixels(GetFrameBufferPixelsCmd *cmd, RenderState *state);

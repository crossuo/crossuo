// GPLv3 License
// Copyright (C) 2019 Everton Fernando Patitucci da Silva

#pragma once

#define NEW_RENDERER_ENABLED

#include "RenderTypes.h"
#include "RenderCommands.h"

RenderState Render_DefaultState();
void Render_ResetCmdList(RenderCmdList *cmdList, RenderState state);

bool Render_CreateShaderPipeline(
    const char *vertexShaderSource, const char *fragmentShaderSource, ShaderPipeline *pipeline);
bool Render_DestroyShaderPipeline(ShaderPipeline *pipeline);
uint32_t Render_GetUniformId(ShaderPipeline *pipeline, const char *uniform);

texture_handle_t Render_CreateTexture2D(
    uint32_t width,
    uint32_t height,
    RenderTextureGPUFormat gpuFormat,
    void *pixels,
    RenderTextureFormat pixelsFormat);
bool Render_DestroyTexture(texture_handle_t texture);

bool RenderAdd_SetTexture(RenderCmdList *cmdList, SetTextureCmd *cmd);
bool RenderAdd_DrawQuad(RenderCmdList *cmdList, DrawQuadCmd *cmds, uint32_t cmd_count);
bool RenderAdd_DrawRotatedQuad(
    RenderCmdList *cmdList, DrawRotatedQuadCmd *cmds, uint32_t cmd_count);
bool RenderAdd_DrawCharacterSitting(RenderCmdList *cmdList, DrawCharacterSittingCmd *cmd);

bool RenderAdd_SetBlend(RenderCmdList *cmdList, BlendStateCmd *state);
bool RenderAdd_DisableBlend(RenderCmdList *cmdList);
bool RenderAdd_SetStencil(RenderCmdList *cmdList, StencilStateCmd *state);
bool RenderAdd_DisableStencil(RenderCmdList *cmdList);
bool RenderAdd_SetColorMask(RenderCmdList *cmdList, SetColorMaskCmd *cmd);
bool RenderAdd_ClearRT(RenderCmdList *cmdList, ClearRTCmd *cmd);

bool RenderAdd_SetShaderUniform(RenderCmdList *cmdList, ShaderUniformCmd *cmd);
bool RenderAdd_SetShaderLargeUniform(RenderCmdList *cmdList, ShaderLargeUniformCmd *cmd);
bool RenderAdd_SetShaderPipeline(RenderCmdList *cmdList, ShaderPipeline *pipeline);
bool RenderAdd_DisableShaderPipeline(RenderCmdList *cmdList);
bool RenderAdd_FlushState(RenderCmdList *cmdList);

bool RenderDraw_Execute(RenderCmdList *cmdList);

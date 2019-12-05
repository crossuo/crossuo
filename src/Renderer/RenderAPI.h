// GPLv3 License
// Copyright (C) 2019 Everton Fernando Patitucci da Silva

#pragma once

#include <SDL.h>
// #define NEW_RENDERER_ENABLED

#if !defined(NEW_RENDERER_ENABLED)
#include "../GLEngine/GLHeaders.h"
#endif
#include "../GLEngine/GLEngine.h" // REMOVE

#include "RenderTypes.h"
#include "RenderCommands.h"

// TODO fix it
bool HACKRender_SetViewParams(const SetViewParamsCmd &cmd);
bool HACKRender_GetFrameBuffer(RenderCmdList *cmdList, frame_buffer_t *currFb);

bool Render_Init(SDL_Window *window);
void Render_Shutdown();
void Render_SwapBuffers();

RenderState Render_DefaultState();
void Render_ResetCmdList(RenderCmdList *cmdList, RenderState state);

bool Render_CreateShaderPipeline(
    const char *vertexShaderSource, const char *fragmentShaderSource, ShaderPipeline *pipeline);
bool Render_DestroyShaderPipeline(ShaderPipeline *pipeline);
uint32_t Render_GetUniformId(ShaderPipeline *pipeline, const char *uniform);

texture_handle_t Render_CreateTexture2D(
    uint32_t width,
    uint32_t height,
    TextureGPUFormat gpuFormat,
    void *pixels,
    TextureFormat pixelsFormat);
bool Render_DestroyTexture(texture_handle_t texture);

frame_buffer_t Render_CreateFrameBuffer(uint32_t width, uint32_t height);
bool Render_DestroyFrameBuffer(frame_buffer_t fb);

bool RenderAdd_SetTexture(RenderCmdList *cmdList, const SetTextureCmd &cmd);
bool RenderAdd_SetFrameBuffer(RenderCmdList *cmdList, const SetFrameBufferCmd &cmd);
bool RenderAdd_DrawQuad(RenderCmdList *cmdList, const DrawQuadCmd &cmd);
bool RenderAdd_DrawRotatedQuad(RenderCmdList *cmdList, const DrawRotatedQuadCmd &cmd);
bool RenderAdd_DrawCharacterSitting(RenderCmdList *cmdList, const DrawCharacterSittingCmd &cmd);
bool RenderAdd_DrawLandTile(RenderCmdList *cmdList, const DrawLandTileCmd &cmd);
bool RenderAdd_DrawShadow(RenderCmdList *cmdList, const DrawShadowCmd &cmd);
bool RenderAdd_DrawCircle(RenderCmdList *cmdList, const DrawCircleCmd &cmd);
bool RenderAdd_DrawUntexturedQuad(RenderCmdList *cmdList, const DrawUntexturedQuadCmd &cmd);
bool RenderAdd_DrawLine(RenderCmdList *cmdList, const DrawLineCmd &cmd);

bool RenderAdd_SetAlphaTest(RenderCmdList *cmdList, const AlphaTestCmd &cmd);
bool RenderAdd_DisableAlphaTest(RenderCmdList *cmdList);
bool RenderAdd_SetBlend(RenderCmdList *cmdList, const BlendStateCmd &cmd);
bool RenderAdd_DisableBlend(RenderCmdList *cmdList);
bool RenderAdd_SetStencil(RenderCmdList *cmdList, const StencilStateCmd &cmd);
bool RenderAdd_DisableStencil(RenderCmdList *cmdList);
bool RenderAdd_EnableStencil(RenderCmdList *cmdList);
bool RenderAdd_SetDepth(RenderCmdList *cmdList, const DepthStateCmd &cmd);
bool RenderAdd_DisableDepth(RenderCmdList *cmdList);
bool RenderAdd_EnableDepth(RenderCmdList *cmdList);
bool RenderAdd_SetColorMask(RenderCmdList *cmdList, const SetColorMaskCmd &cmd);
bool RenderAdd_SetColor(RenderCmdList *cmdList, const SetColorCmd &cmd);
bool RenderAdd_SetClearColor(RenderCmdList *cmdList, const SetClearColorCmd &cmd);
bool RenderAdd_ClearRT(RenderCmdList *cmdList, const ClearRTCmd &cmd);

bool RenderAdd_SetShaderUniform(RenderCmdList *cmdList, const ShaderUniformCmd &cmd);
bool RenderAdd_SetShaderLargeUniform(RenderCmdList *cmdList, const ShaderLargeUniformCmd &cmd);
bool RenderAdd_SetShaderPipeline(RenderCmdList *cmdList, ShaderPipeline *pipeline);
bool RenderAdd_DisableShaderPipeline(RenderCmdList *cmdList);
bool RenderAdd_FlushState(RenderCmdList *cmdList);
bool RenderAdd_SetViewParams(RenderCmdList *cmdList, const SetViewParamsCmd &cmd);
bool RenderAdd_SetModelViewTranslation(
    RenderCmdList *cmdList, const SetModelViewTranslationCmd &cmd);
bool RenderAdd_SetScissor(RenderCmdList *cmdList, const SetScissorCmd &cmd);
bool RenderAdd_DisableScissor(RenderCmdList *cmdList);

bool RenderAdd_GetFrameBufferPixels(RenderCmdList *cmdList, const GetFrameBufferPixelsCmd &cmd);

bool RenderDraw_Execute(RenderCmdList *cmdList);

#pragma once

#ifndef RENDERER_INTERNAL
#error "Do not include this directly. Include RenderAPI.h"
#endif

bool Render_AppendCmd(RenderCmdList *cmdList, void *cmd, uint32_t cmdSize);
bool RenderState_SetTexture(
    RenderState *state, RenderTextureType type, textureHandle_t texture, bool forced = false);
bool RenderState_FlushState(RenderState *state);
bool RenderState_SetBlend(RenderState *state, bool enabled, BlendFunc func, bool forced = false);
bool RenderState_SetStencil(RenderState *state, bool enabled, bool forced = false);

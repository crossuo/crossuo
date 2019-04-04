#include "Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "Renderer/RenderInternal.h"

TextureCmd RenderAdd_TextureCmd(
    textureHandle_t texture,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    float u,
    float v)
{
    return TextureCmd(texture, x, y, width, height, u, v);
}

bool RenderAdd_Texture(RenderCmdList *cmdList, TextureCmd *textures, uint32_t texture_count)
{
    return Render_AppendCmd(cmdList, textures, texture_count * sizeof(TextureCmd));
}

BlendStateCmd RenderAdd_Blend(bool enabled, BlendFunc func)
{
    return BlendStateCmd(enabled, func);
}

bool RenderAdd_SetBlend(RenderCmdList *cmdList, BlendStateCmd *state)
{
    return Render_AppendCmd(cmdList, state, sizeof(BlendStateCmd));
}

StencilStateCmd RenderAdd_Stencil(bool enabled)
{
    return StencilStateCmd(enabled);
}

bool RenderAdd_SetStencil(RenderCmdList *cmdList, StencilStateCmd *state)
{
    return Render_AppendCmd(cmdList, state, sizeof(StencilStateCmd));
}

bool RenderAdd_FlushState(RenderCmdList *cmdList, RenderState *state)
{
    return Render_AppendCmd(cmdList, &FlushStateCmd(), sizeof(FlushStateCmd));
}

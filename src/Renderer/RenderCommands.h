// GPLv3 License
// Copyright (C) 2019 Everton Fernando Patitucci da Silva

#pragma once

enum RenderCommandType : uint8_t
{
    Cmd_DrawQuad = 0,
    Cmd_DrawRotatedQuad,
    Cmd_DrawCharacterSitting,
    Cmd_ClearRT,

    Cmd_FlushState,

    Cmd_SetTexture,
    Cmd_BlendState,
    Cmd_DisableBlendState,
    Cmd_StencilState,
    Cmd_DisableStencilState,
    Cmd_SetColorMask,

    Cmd_ShaderUniform,
    Cmd_ShaderLargeUniform,
    Cmd_ShaderPipeline,
    Cmd_DisableShaderPipeline,
};

struct RenderCommandHeader
{
    RenderCommandType type = RenderCommandType{ 0 };
    RenderCommandHeader(RenderCommandType type)
        : type(type)
    {
    }

    RenderCommandHeader() = default;
};

struct SetTextureCmd
{
    RenderCommandHeader header;
    texture_handle_t texture;
    RenderTextureType type;

    SetTextureCmd(texture_handle_t texture, RenderTextureType type)
        : header{ RenderCommandType::Cmd_SetTexture }
        , texture(texture)
        , type(type)
    {
    }
};

struct DrawQuadCmd
{
    RenderCommandHeader header;
    texture_handle_t texture;
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    float u = 1.f;
    float v = 1.f;
    float4 rgba = g_ColorWhite;
    bool mirrored = false;

    DrawQuadCmd() = default;

    DrawQuadCmd(
        texture_handle_t texture,
        int32_t x,
        int32_t y,
        uint32_t width,
        uint32_t height,
        float u = 1.f,
        float v = 1.f,
        float4 rgba = g_ColorWhite,
        bool mirrored = false)
        : header{ RenderCommandType::Cmd_DrawQuad }
        , texture(texture)
        , x(x)
        , y(y)
        , width(width)
        , height(height)
        , u(u)
        , v(v)
        , rgba(rgba)
        , mirrored(mirrored)
    {
    }
};

struct DrawCharacterSittingCmd
{
    RenderCommandHeader header;
    texture_handle_t texture;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    float h3mod;
    float h6mod;
    float h9mod;
    bool mirror;

    DrawCharacterSittingCmd(
        texture_handle_t texture,
        int32_t x,
        int32_t y,
        uint32_t width,
        uint32_t height,
        bool mirror,
        float h3mod,
        float h6mod,
        float h9mod)
        : header{ RenderCommandType::Cmd_DrawCharacterSitting }
        , texture(texture)
        , x(x)
        , y(y)
        , width(width)
        , height(height)
        , h3mod(h3mod)
        , h6mod(h6mod)
        , h9mod(h9mod)
        , mirror(mirror)
    {
    }
};

struct DrawRotatedQuadCmd : public DrawQuadCmd
{
    float angle;

    DrawRotatedQuadCmd(
        texture_handle_t texture,
        uint32_t x,
        uint32_t y,
        uint32_t width,
        uint32_t height,
        float angle,
        float u = 1.f,
        float v = 1.f,
        float4 rgba = g_ColorWhite,
        bool mirrored = false)
        : DrawQuadCmd(texture, x, y, width, height, u, v, rgba, mirrored)
        , angle(angle)
    {
        header.type = RenderCommandType::Cmd_DrawRotatedQuad;
    }
};

struct BlendStateCmd
{
    RenderCommandHeader header;
    BlendFunc func = BlendFunc{ 0 };

    BlendStateCmd(BlendFunc func)
        : header{ RenderCommandType::Cmd_BlendState }
        , func(func)
    {
    }
};

struct DisableBlendStateCmd
{
    RenderCommandHeader header;
    DisableBlendStateCmd()
        : header{ RenderCommandType::Cmd_DisableBlendState }
    {
    }
};

struct StencilStateCmd
{
    RenderCommandHeader header;
    StencilFunc func;
    StencilOp stencilFail;
    StencilOp depthFail;
    StencilOp bothFail;
    uint32_t ref;
    uint32_t mask;

    StencilStateCmd(
        StencilFunc func = NeverPass,
        uint32_t ref = 0,
        uint32_t mask = 0xffffffff,
        StencilOp stencilFail = Keep,
        StencilOp depthFail = Keep,
        StencilOp bothFail = Keep)
        : header{ RenderCommandType::Cmd_StencilState }
        , func(func)
        , stencilFail(stencilFail)
        , depthFail(depthFail)
        , bothFail(bothFail)
        , ref(ref)
        , mask(mask)
    {
    }
};

struct DisableStencilStateCmd
{
    RenderCommandHeader header;
    DisableStencilStateCmd()
        : header{ RenderCommandType::Cmd_DisableStencilState }
    {
    }
};

struct SetColorMaskCmd
{
    RenderCommandHeader header;
    ColorMask mask;
    SetColorMaskCmd(ColorMask mask = ColorMask::ColorMask_All)
        : header{ RenderCommandType::Cmd_SetColorMask }
        , mask(mask)
    {
    }
};

struct ClearRTCmd
{
    RenderCommandHeader header;
    ClearRT clearMask;
    ClearRTCmd(ClearRT clearMask = ClearRT::ClearRT_All)
        : header{ RenderCommandType::Cmd_ClearRT }
        , clearMask(clearMask)
    {
    }
};

struct FlushStateCmd
{
    RenderCommandHeader header;
    FlushStateCmd()
        : header{ RenderCommandType::Cmd_FlushState }
    {
    }
};

struct ShaderUniformCmd
{
    RenderCommandHeader header;
    uint32_t id;
    uint8_t value[RENDERSTATE_SHADER_UNIFORMDATA_SIZE];
    ShaderUniformType type;

    ShaderUniformCmd(uint32_t id, void *value, ShaderUniformType type);
};

struct ShaderLargeUniformCmd
{
    RenderCommandHeader header;
    void *value;
    uint32_t id;
    uint32_t count;
    ShaderUniformType type;

    ShaderLargeUniformCmd(uint32_t id, void *value, uint32_t count, ShaderUniformType type)
        : header{ RenderCommandType::Cmd_ShaderLargeUniform }
        , value(value)
        , id(id)
        , count(count)
        , type(type)
    {
    }
};

struct ShaderPipelineCmd
{
    RenderCommandHeader header;
    ShaderPipeline *pipeline;

    ShaderPipelineCmd(ShaderPipeline *pipeline)
        : header{ RenderCommandType::Cmd_ShaderPipeline }
        , pipeline(pipeline)
    {
    }
};

struct DisableShaderPipelineCmd
{
    RenderCommandHeader header;

    DisableShaderPipelineCmd()
        : header{ RenderCommandType::Cmd_DisableShaderPipeline }
    {
    }
};

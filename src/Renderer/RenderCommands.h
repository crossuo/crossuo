// GPLv3 License
// Copyright (C) 2019 Everton Fernando Patitucci da Silva

#pragma once

enum RenderCommandType : uint8_t
{
    Cmd_DrawQuad = 0,
    Cmd_DrawRotatedQuad,
    Cmd_DrawCharacterSitting,
    Cmd_DrawLandTile,
    Cmd_DrawShadow,
    Cmd_DrawCircle,
    Cmd_DrawUntexturedQuad,
    Cmd_DrawLine,
    Cmd_ClearRT,

    Cmd_FlushState,

    Cmd_SetTexture,
    Cmd_SetFrameBuffer,
    Cmd_AlphaTest,
    Cmd_DisableAlphaTest,
    Cmd_BlendState,
    Cmd_DisableBlendState,
    Cmd_StencilState,
    Cmd_DisableStencilState,
    Cmd_EnableStencilState,
    Cmd_SetColorMask,
    Cmd_SetColor,
    Cmd_SetClearColor,
    Cmd_SetViewParams,
    Cmd_SetModelViewTranslation,
    Cmd_SetScissor,
    Cmd_DisableScissor,

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

struct SetFrameBufferCmd
{
    RenderCommandHeader header;
    frame_buffer_t frameBuffer;

    SetFrameBufferCmd(frame_buffer_t frameBuffer)
        : header{ RenderCommandType::Cmd_SetFrameBuffer }
        , frameBuffer(frameBuffer)
    {
    }
};

struct SetViewParamsCmd
{
    RenderCommandHeader header;
    int scene_x;
    int scene_y;
    int scene_width;
    int scene_height;
    int window_width;
    int window_height;
    int camera_nearZ;
    int camera_farZ;
    float scene_scale;
    bool proj_flipped_y;

    SetViewParamsCmd(
        int scene_x,
        int scene_y,
        int scene_width,
        int scene_height,
        int window_width,
        int window_height,
        int camera_nearZ,
        int camera_farZ,
        float scene_scale = 1.f,
        bool proj_flipped_y = false)
        : header{ RenderCommandType::Cmd_SetViewParams }
        , scene_x(scene_x)
        , scene_y(scene_y)
        , scene_width(scene_width)
        , scene_height(scene_height)
        , window_width(window_width)
        , window_height(window_height)
        , camera_nearZ(camera_nearZ)
        , camera_farZ(camera_farZ)
        , scene_scale(scene_scale)
        , proj_flipped_y(proj_flipped_y)
    {
    }
};

struct SetModelViewTranslationCmd
{
    RenderCommandHeader header;
    float3 pos;

    SetModelViewTranslationCmd(float3 pos)
        : header{ RenderCommandType::Cmd_SetModelViewTranslation }
        , pos(pos)
    {
    }
};

struct SetScissorCmd
{
    RenderCommandHeader header;
    int x;
    int y;
    uint32_t width;
    uint32_t height;

    SetScissorCmd(int x, int y, uint32_t width, uint32_t height)
        : header{ RenderCommandType::Cmd_SetScissor }
        , x(x)
        , y(y)
        , width(width)
        , height(height)
    {
    }
};

struct DisableScissorCmd
{
    RenderCommandHeader header;

    DisableScissorCmd()
        : header{ RenderCommandType::Cmd_DisableScissor }
    {
    }
};

struct DrawQuadCmd
{
    RenderCommandHeader header;
    texture_handle_t texture;
    int x = 0;
    int y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    float u = 1.f;
    float v = 1.f;
    float4 rgba = g_ColorWhite;
    bool mirrored = false;

    DrawQuadCmd() = default;

    DrawQuadCmd(
        texture_handle_t texture,
        int x,
        int y,
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
    int x;
    int y;
    uint32_t width;
    uint32_t height;
    float h3mod;
    float h6mod;
    float h9mod;
    bool mirror;

    DrawCharacterSittingCmd(
        texture_handle_t texture,
        int x,
        int y,
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

struct DrawLandTileCmd
{
    RenderCommandHeader header;
    texture_handle_t texture;
    int x;
    int y;
    struct
    {
        int x;
        int y;
        uint32_t width;
        uint32_t height;
    } rect;
    float3 normals[4];

    DrawLandTileCmd(
        texture_handle_t texture,
        int x,
        int y,
        int rect_x,
        int rect_y,
        uint32_t rect_w,
        uint32_t rect_h,
        float3 normals[4])
        : header{ RenderCommandType::Cmd_DrawLandTile }
        , texture(texture)
        , x(x)
        , y(y)
        , rect{ rect_x, rect_y, rect_w, rect_h }
        , normals{ normals[0], normals[1], normals[2], normals[3] }
    {
    }
};

struct DrawShadowCmd
{
    RenderCommandHeader header;
    texture_handle_t texture;
    int x;
    int y;
    int uniformValue;
    uint32_t width;
    uint32_t height;
    uint32_t uniformId;
    bool mirror;
    bool restoreBlendFunc;

    DrawShadowCmd(
        texture_handle_t texture,
        int x,
        int y,
        uint32_t width,
        uint32_t height,
        bool mirror,
        uint32_t uniformId,
        int uniformValue,
        bool restoreBlendFunc)
        : header{ RenderCommandType::Cmd_DrawShadow }
        , texture(texture)
        , x(x)
        , y(y)
        , uniformValue(uniformValue)
        , width(width)
        , height(height)
        , uniformId(uniformId)
        , mirror(mirror)
        , restoreBlendFunc(restoreBlendFunc)
    {
    }
};

struct DrawCircleCmd
{
    RenderCommandHeader header;
    int x;
    int y;
    float radius;
    int gradientMode;
    DrawCircleCmd(int x, int y, float radius, int gradientMode = 0)
        : header{ RenderCommandType::Cmd_DrawCircle }
        , x(x)
        , y(y)
        , radius(radius)
        , gradientMode(gradientMode)
    {
    }
};

struct DrawUntexturedQuadCmd
{
    RenderCommandHeader header;
    int x;
    int y;
    uint32_t width;
    uint32_t height;
    float4 color;
    DrawUntexturedQuadCmd(
        int x, int y, uint32_t width, uint32_t height, float4 color = g_ColorInvalid)
        : header{ RenderCommandType::Cmd_DrawUntexturedQuad }
        , x(x)
        , y(y)
        , width(width)
        , height(height)
        , color(color)
    {
    }
};

struct DrawLineCmd
{
    RenderCommandHeader header;
    int x0;
    int y0;
    int x1;
    int y1;
    float4 color;

    DrawLineCmd(int x0, int y0, int x1, int y1, float4 color = g_ColorInvalid)
        : header{ RenderCommandType::Cmd_DrawLine }
        , x0(x0)
        , y0(y0)
        , x1(x1)
        , y1(y1)
        , color(color)
    {
    }
};

struct DrawRotatedQuadCmd : public DrawQuadCmd
{
    float angle;

    DrawRotatedQuadCmd(
        texture_handle_t texture,
        int x,
        int y,
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

struct AlphaTestCmd
{
    RenderCommandHeader header;
    AlphaTestFunc func = AlphaTestFunc::AlphaTest_Invalid;
    float ref;

    AlphaTestCmd(AlphaTestFunc func, float ref)
        : header{ RenderCommandType::Cmd_AlphaTest }
        , func(func)
        , ref(ref)
    {
    }
};

struct DisableAlphaTestCmd
{
    RenderCommandHeader header;

    DisableAlphaTestCmd()
        : header{ RenderCommandType::Cmd_DisableAlphaTest }
    {
    }
};

struct BlendStateCmd
{
    RenderCommandHeader header;
    BlendFunc func = BlendFunc::BlendFunc_Invalid;

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
        StencilFunc func = StencilFunc::StencilFunc_NeverPass,
        uint32_t ref = 0,
        uint32_t mask = 0xffffffff,
        StencilOp stencilFail = StencilOp::Keep,
        StencilOp depthFail = StencilOp::Keep,
        StencilOp bothFail = StencilOp::Keep)
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

struct EnableStencilStateCmd
{
    RenderCommandHeader header;
    EnableStencilStateCmd()
        : header{ RenderCommandType::Cmd_EnableStencilState }
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

struct SetColorCmd
{
    RenderCommandHeader header;
    float4 color;

    SetColorCmd(float4 color)
        : header{ RenderCommandType::Cmd_SetColor }
        , color(color)
    {
    }
};

struct SetClearColorCmd
{
    RenderCommandHeader header;
    float4 color;

    SetClearColorCmd(float4 color)
        : header{ RenderCommandType::Cmd_SetClearColor }
        , color(color)
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

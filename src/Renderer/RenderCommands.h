// AGPLv3 License
// Copyright (C) 2019 Everton Fernando Patitucci da Silva

#pragma once

#include <stdint.h>

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
    Cmd_DepthState,
    Cmd_DisableDepthState,
    Cmd_EnableDepthState,
    Cmd_SetColorMask,
    Cmd_SetColor,
    Cmd_SetClearColor,
    Cmd_SetViewParams,
    Cmd_SetModelViewTranslation,
    Cmd_SetScissor,
    Cmd_DisableScissor,
    Cmd_GetFrameBufferPixels,

    Cmd_ShaderUniform,
    Cmd_ShaderLargeUniform,
    Cmd_ShaderPipeline,
    Cmd_DisableShaderPipeline,

    RenderCommandType_Invalid = 0xff,
};

struct RenderCommandHeader
{
    RenderCommandType type = RenderCommandType_Invalid;
    RenderCommandHeader(RenderCommandType type_)
        : type(type_)
    {
    }

    RenderCommandHeader() = default;
};

struct SetTextureCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_SetTexture;
    texture_handle_t texture = RENDER_TEXTUREHANDLE_INVALID;
    TextureType type = TextureType::TextureType_Invalid;
};

struct SetFrameBufferCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_SetFrameBuffer;
    frame_buffer_t frameBuffer{};
};

struct SetViewParamsCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_SetViewParams;
    int scene_x = 0;
    int scene_y = 0;
    int scene_width = 0;
    int scene_height = 0;
    int window_width = 0;
    int window_height = 0;
    int camera_nearZ = 0;
    int camera_farZ = 0;
    float scene_scale = 1.f;
    bool proj_flipped_y = false;
};

struct SetModelViewTranslationCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_SetModelViewTranslation;
    float3 pos{ 0.f, 0.f, 0.f };
};

struct SetScissorCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_SetScissor;
    int x = 0;
    int y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct DisableScissorCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_DisableScissor;
};

struct DrawQuadCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_DrawQuad;
    texture_handle_t texture = RENDER_TEXTUREHANDLE_INVALID;
    int x = 0;
    int y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    float u = 1.f;
    float v = 1.f;
    float4 rgba = g_ColorInvalid;
    bool mirrored = false;
};

struct DrawCharacterSittingCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_DrawCharacterSitting;
    texture_handle_t texture = RENDER_TEXTUREHANDLE_INVALID;
    int x = 0;
    int y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    float h3mod = 0.f;
    float h6mod = 0.f;
    float h9mod = 0.f;
    bool mirror = false;
};

struct DrawLandTileCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_DrawLandTile;
    texture_handle_t texture = RENDER_TEXTUREHANDLE_INVALID;
    int x = 0;
    int y = 0;
    struct
    {
        int x = 0;
        int y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
    } rect;
    float3 normals[4];
};

struct DrawShadowCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_DrawShadow;
    texture_handle_t texture = RENDER_TEXTUREHANDLE_INVALID;
    int x = 0;
    int y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t uniformId = 0;
    int uniformValue = 0;
    bool mirror = false;
    bool restoreBlendFunc = false;
};

struct DrawCircleCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_DrawCircle;
    int x = 0;
    int y = 0;
    float radius = 0.f;
    int gradientMode = 0;
};

struct DrawUntexturedQuadCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_DrawUntexturedQuad;
    int x = 0;
    int y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    float4 color = g_ColorInvalid;
};

struct DrawLineCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_DrawLine;
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    float4 color = g_ColorInvalid;
};

struct DrawRotatedQuadCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_DrawRotatedQuad;
    texture_handle_t texture = RENDER_TEXTUREHANDLE_INVALID;
    int x = 0;
    int y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    float angle = 0.f;
    float u = 1.f;
    float v = 1.f;
    float4 rgba = g_ColorWhite;
    bool mirrored = false;
};

struct AlphaTestCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_AlphaTest;
    AlphaTestFunc func = AlphaTestFunc::AlphaTestFunc_Invalid;
    float ref = 0.f;
};

struct DisableAlphaTestCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_DisableAlphaTest;
};

struct BlendStateCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_BlendState;
    BlendFactor src = BlendFactor::BlendFactor_Invalid;
    BlendFactor dst = BlendFactor::BlendFactor_Invalid;
    BlendEquation equation = BlendEquation::BlendEquation_Add;
};

struct DisableBlendStateCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_DisableBlendState;
};

struct StencilStateCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_StencilState;
    StencilFunc func = StencilFunc::StencilFunc_NeverPass;
    StencilOp stencilFail = StencilOp::StencilOp_Keep;
    StencilOp depthFail = StencilOp::StencilOp_Keep;
    StencilOp bothFail = StencilOp::StencilOp_Keep;
    uint32_t ref = 0;
    uint32_t mask = 0xffffffff;
};

struct DisableStencilStateCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_DisableStencilState;
};

struct EnableStencilStateCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_EnableStencilState;
};

struct DepthStateCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_DepthState;
    DepthFunc func = DepthFunc::DepthFunc_Invalid;
};

struct DisableDepthStateCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_DisableDepthState;
};

struct EnableDepthStateCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_EnableDepthState;
};

struct SetColorMaskCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_SetColorMask;
    ColorMask mask = ColorMask::ColorMask_All;
};

struct SetColorCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_SetColor;
    float4 color = g_ColorInvalid;
};

struct SetClearColorCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_SetClearColor;
    float4 color = g_ColorInvalid;
};

struct ClearRTCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_ClearRT;
    ClearRT clearMask = ClearRT::ClearRT_All;
};

struct FlushStateCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_FlushState;
};

struct ShaderUniformCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_ShaderUniform;
    uint32_t id = RENDER_SHADERUNIFORMID_INVALID;
    ShaderUniformType uniform_type = ShaderUniformType::ShaderUniformType_Invalid;
    union {
        uint8_t data
            [RENDERSTATE_SHADER_UNIFORMDATA_SIZE]{}; // TODO add uniform storage for anything larger than this
        int asInt1;
        int asInt2[2];
        int asInt3[3];
        int asInt4[4];
        float asFloat1;
        float asFloat2[2];
        float asFloat3[3];
        float asFloat4[4];
    } value{};
};

struct ShaderLargeUniformCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_ShaderLargeUniform;
    void *value =
        nullptr; // TODO add uniform storage during uniform init/setup and use that for larger uniforms
    uint32_t count = 0;
    uint32_t id = RENDER_SHADERUNIFORMID_INVALID;
    ShaderUniformType uniform_type = ShaderUniformType::ShaderUniformType_Invalid;
};

struct ShaderPipelineCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_ShaderPipeline;
    ShaderPipeline *pipeline = nullptr;
};

struct DisableShaderPipelineCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_DisableShaderPipeline;
};

struct GetFrameBufferPixelsCmd
{
    static constexpr RenderCommandType _type = RenderCommandType::Cmd_GetFrameBufferPixels;
    int x = 0;
    int y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t window_width = 0;
    uint32_t window_height = 0;
    void *data = nullptr; // TODO add resource storage to the renderer, this would be a handle then
    size_t dataSize = 0;
};

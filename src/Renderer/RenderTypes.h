// AGPLv3 License
// Copyright (C) 2019 Everton Fernando Patitucci da Silva

#pragma once

#include <stdint.h>
#include <common/logging/logging.h>

#define RENDER_TEXTUREHANDLE_INVALID (0xffffffff)
#define RENDER_SHADERUNIFORMID_INVALID (0xffffffff)
#define RENDER_SHADERUNIFORMLOC_INVALID (0xffffffff)
#define RENDER_SHADERPROGRAM_INVALID (0xffffffff)
#define RENDER_SHADERHANDLE_INVALID (0xffffffff)
#define RENDER_FRAMEBUFFER_INVALID (0xffffffff)

#define RENDER_MAX_SHADERPIPELINE_UNIFORM (3)
#define RENDERSTATE_MAX_SHADER_UNIFORM (3)
#define RENDERSTATE_SHADER_UNIFORMDATA_SIZE                                                        \
    (4 * sizeof(uint32_t)) // max size for possible values of glUniform{1|2|3|4}{f|i|ui}

typedef uint32_t texture_handle_t;
typedef uint32_t framebuffer_handle_t;
typedef uint32_t shaderprogram_handle_t;
typedef uint32_t shader_handle_t;

struct frame_buffer_t
{
    texture_handle_t texture = RENDER_TEXTUREHANDLE_INVALID;
    framebuffer_handle_t handle = RENDER_FRAMEBUFFER_INVALID;
};

// TODO simd-friendly types
// TODO accessors (x(), xxxx(), ...)
struct float4
{
    float4(float r, float g, float b, float a)
        : rgba{ r, g, b, a }
    {
    }
    float rgba[4];

    float operator[](size_t i) const;
    bool operator==(float4 &other);
    bool operator!=(float4 &other);
};

struct float3
{
    float3(float r, float g, float b)
        : rgb{ r, g, b }
    {
    }
    float rgb[3] = { 0.f, 0.f, 0.f };

    float operator[](size_t i) const;
    bool operator==(float3 &other);
    bool operator!=(float3 &other);
};

// TODO simd-friendly types
// Rect

extern float4 g_ColorWhite;
extern float4 g_ColorBlack;
extern float4 g_ColorBlue;
extern float4 g_ColorInvalid;

enum BlendFactor : uint8_t
{
    BlendFactor_Zero = 0,
    BlendFactor_One,
    BlendFactor_OneMinusSrcAlpha,
    BlendFactor_OneMinusSrcColor,
    BlendFactor_SrcColor,
    BlendFactor_SrcAlpha,
    BlendFactor_DstColor,

    BlendFactor_Count,
    BlendFactor_Invalid = 0xff,
};

enum BlendEquation : uint8_t
{
    BlendEquation_Add = 0,
    BlendEquation_ReverseSubtract,

    BlendEquation_Count,
    BlendEquation_Invalid = 0xff,
};

enum StencilFunc : uint8_t
{
    StencilFunc_NeverPass = 0,
    StencilFunc_AlwaysPass,
    StencilFunc_Less,
    StencilFunc_LessOrEqual,
    StencilFunc_Greater,
    StencilFunc_GreaterOrEqual,
    StencilFunc_Equal,
    StencilFunc_Different,

    StencilFunc_Count,
    StencilFunc_Invalid = 0xff,
};

enum StencilOp : uint8_t
{
    StencilOp_Keep = 0,
    StencilOp_Zero,
    StencilOp_Replace,
    StencilOp_IncrementClamp,
    StencilOp_IncrementWrap,
    StencilOp_DecrementClamp,
    StencilOp_DecrementWrap,
    StencilOp_Invert,

    StencilOp_Count,
    StencilOp_Invalid = 0xff,
};

enum DepthFunc : uint8_t
{
    DepthFunc_NeverPass = 0,
    DepthFunc_AlwaysPass,
    DepthFunc_Equal,
    DepthFunc_Different,
    DepthFunc_Less,
    DepthFunc_LessOrEqual,
    DepthFunc_Greater,
    DepthFunc_GreaterOrEqual,

    DepthFunc_Count,
    DepthFunc_Invalid = 0xff,
};

enum AlphaTestFunc : uint8_t
{
    AlphaTestFunc_NeverPass = 0,
    AlphaTestFunc_AlwaysPass,
    AlphaTestFunc_Equal,
    AlphaTestFunc_Different,
    AlphaTestFunc_Less,
    AlphaTestFunc_LessOrEqual,
    AlphaTestFunc_Greater,
    AlphaTestFunc_GreaterOrEqual,

    AlphaTestFunc_Count,
    AlphaTestFunc_Invalid = 0xff,
};

enum ColorMask : uint8_t
{
    ColorMask_Red = (1 << 0),
    ColorMask_Green = (1 << 1),
    ColorMask_Blue = (1 << 2),
    ColorMask_Alpha = (1 << 3),

    ColorMask_All = ColorMask_Red | ColorMask_Green | ColorMask_Blue | ColorMask_Alpha,
    ColorMask_ColorOnly = ColorMask_Red | ColorMask_Green | ColorMask_Blue,
    ColorMask_None = 0,
    ColorMask_Invalid = 0xff,
};

enum ClearRT : uint8_t
{
    ClearRT_Color = (1 << 0),
    ClearRT_Depth = (1 << 1),
    ClearRT_Stencil = (1 << 2),

    ClearRT_All = ClearRT_Color | ClearRT_Depth | ClearRT_Stencil,
    ClearRT_Invalid = 0xff,
};

enum TextureType : uint8_t
{
    TextureType_Texture2D = 0,
    TextureType_Texture2D_Mipmapped,

    TextureType_Count,
    TextureType_Invalid = 0xff
};

enum TextureGPUFormat : uint8_t
{
    TextureGPUFormat_RGBA4 = 0,
    TextureGPUFormat_RGB5_A1,

    TextureGPUFormat_Count,
};

enum TextureFormat : uint8_t
{
    TextureFormat_Unsigned_RGBA8 = 0,
    TextureFormat_Unsigned_ABGR8,
    TextureFormat_Unsigned_A1_BGR5,

    TextureFormat_Count,
};

enum ShaderUniformType : uint8_t
{
    ShaderUniformType_Int1 = 0,

    // Large uniforms start (variable size)
    ShaderUniformType_Float1V,

    ShaderUniformType_Count,
    ShaderUniformType_Invalid = 0xff,

    ShaderUniformType_VariableFirst = ShaderUniformType_Float1V,
    ShaderUniformType_VariableLast = ShaderUniformType_Count - 1,
    ShaderUniformType_VariableCount =
        ShaderUniformType_VariableLast - ShaderUniformType_VariableFirst + 1,
    ShaderUniformType_FixedFirst = ShaderUniformType_Int1,
    ShaderUniformType_FixedLast = ShaderUniformType_VariableFirst - 1,
    ShaderUniformType_FixedCount = ShaderUniformType_FixedLast - ShaderUniformType_FixedFirst + 1,
};

enum ShaderStage : uint8_t
{
    ShaderStage_VertexShader = 0,
    ShaderStage_FragmentShader,

    ShaderStage_Count,
    ShaderStage_Invalid = 0xff
};

struct ShaderUniformData
{
    uint32_t location = RENDER_SHADERUNIFORMLOC_INVALID;
};

struct ShaderPipeline
{
    shaderprogram_handle_t program = RENDER_SHADERPROGRAM_INVALID;
    shader_handle_t shaders[ShaderStage::ShaderStage_Count] = { RENDER_SHADERHANDLE_INVALID };
    ShaderUniformData uniforms[RENDER_MAX_SHADERPIPELINE_UNIFORM] = {};
    uint32_t uniformCount = 0;
};

struct RenderStateUniformCache
{
    uint8_t data[RENDERSTATE_MAX_SHADER_UNIFORM][RENDERSTATE_SHADER_UNIFORMDATA_SIZE];
    bool dataSet[RENDERSTATE_MAX_SHADER_UNIFORM];
};

struct RenderState
{
    struct
    {
        texture_handle_t texture;
        TextureType type;
    } texture = { RENDER_TEXTUREHANDLE_INVALID, TextureType::TextureType_Texture2D };

    frame_buffer_t framebuffer{};

    struct
    {
        int left;
        int right;
        int bottom;
        int top;
        int nearZ;
        int farZ;
        float scale;
        bool proj_flipped_y;
    } viewport = { 0, 0, 0, 0, 0, 0, 1.f, false };

    struct
    {
        int x;
        int y;
        uint32_t width;
        uint32_t height;
        bool enabled;
    } scissor = { 0, 0, 0, 0, false };

    ShaderPipeline pipeline;
    RenderStateUniformCache uniformCache;

    struct
    {
        bool enabled;
        BlendFactor src;
        BlendFactor dst;
        BlendEquation equation;
    } blend = { false,
                BlendFactor::BlendFactor_SrcAlpha,
                BlendFactor::BlendFactor_OneMinusSrcAlpha,
                BlendEquation::BlendEquation_Add };

    struct
    {
        bool enabled;
        AlphaTestFunc func;
        float alphaRef;
    } alphaTest = { true, AlphaTestFunc::AlphaTestFunc_Greater, 0.f };

    // TODO support front- and back-face independent stencil testing
    struct
    {
        bool enabled;
        StencilFunc func;
        StencilOp stencilFail;
        StencilOp depthFail;
        StencilOp bothFail;
        uint32_t ref;
        uint32_t mask;
    } stencil = { false,
                  StencilFunc::StencilFunc_NeverPass,
                  StencilOp::StencilOp_Keep,
                  StencilOp::StencilOp_Keep,
                  StencilOp::StencilOp_Keep,
                  0,
                  1 };

    struct
    {
        bool enabled;
        DepthFunc func;
    } depth = { false, DepthFunc::DepthFunc_LessOrEqual };

    ColorMask colorMask = ColorMask::ColorMask_All;
    float4 color = g_ColorWhite;
    float4 clearColor = g_ColorBlack;
};

struct RenderCmdList
{
    RenderState state;
    char *data = nullptr;
    uint32_t size = 0;
    uint32_t remainingSize = 0;
    bool immediateMode = false;

    RenderCmdList(void *data_, uint32_t size_, RenderState state_, bool immediateMode_)
        : state(state_)
        , data((char *)data_)
        , size(size_)
        , remainingSize(size_)
        , immediateMode(immediateMode_)
    {
    }

    RenderCmdList() = default;
};

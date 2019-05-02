// GPLv3 License
// Copyright (C) 2019 Everton Fernando Patitucci da Silva

#pragma once

#define RENDER_TEXTUREHANDLE_INVALID (0xffffffff)
#define RENDER_SHADERUNIFORMID_INVALID (0xffffffff)
#define RENDER_SHADERUNIFORMLOC_INVALID (0xffffffff)
#define RENDER_SHADERPROGRAM_INVALID (0xffffffff)
#define RENDER_SHADERHANDLE_INVALID (0xffffffff)

#define RENDER_MAX_SHADERPIPELINE_UNIFORM (3)
#define RENDERSTATE_MAX_SHADER_UNIFORM (3)
#define RENDERSTATE_SHADER_UNIFORMDATA_SIZE (4 * 4 * sizeof(uint32_t))

typedef uint32_t texture_handle_t;
typedef uint32_t shaderprogram_handle_t;
typedef uint32_t shader_handle_t;

// TODO simd-friendly types
// TODO accessors (x(), xxxx(), ...)
struct float4
{
    float rgba[4];

    float operator[](size_t i);
    bool operator==(float4 &other);
    bool operator!=(float4 &other);
};

struct float3
{
    float rgb[3];

    float operator[](size_t i);
    bool operator==(float3 &other);
    bool operator!=(float3 &other);
};

// TODO simd-friendly types
// Rect

extern float4 g_ColorWhite;
extern float4 g_ColorBlack;
extern float4 g_ColorInvalid;

enum BlendFunc : uint8_t
{
    SrcAlpha_OneMinusSrcAlpha = 0,
    One_OneMinusSrcAlpha,
    DstColor_Zero,
    Zero_OneMinusSrcAlpha,
    Zero_SrcColor,

    BlendFunc_Count,
    BlendFunc_Invalid = 0xff,
};

enum StencilFunc : uint8_t
{
    NeverPass = 0,
    Less,
    LessOrEqual,
    Greater,
    GreaterOrEqual,
    Equal,
    Different,
    AlwaysPass,

    StencilFunc_Count,
    StencilFunc_Invalid = 0xff,
};

enum StencilOp : uint8_t
{
    Keep = 0,
    Zero,
    Replace,
    IncrementClamp,
    IncrementWrap,
    DecrementClamp,
    DecrementWrap,
    Invert,

    StencilOp_Count,
    StencilOp_Invalid = 0xff,
};

enum ColorMask : uint8_t
{
    Red = (1 << 0),
    Green = (1 << 1),
    Blue = (1 << 2),
    Alpha = (1 << 3),

    ColorMask_All = Red | Green | Blue | Alpha,
    ColorMask_ColorOnly = Red | Green | Blue,
    ColorMask_None = 0,
    ColorMask_Invalid = 0xff,
};

enum ClearRT : uint8_t
{
    Color = (1 << 0),
    Depth = (1 << 1),
    Stencil = (1 << 2),

    ClearRT_All = Color | Depth | Stencil,
    ClearRT_Invalid = 0xff,
};

enum RenderTextureType : uint8_t
{
    Texture2D = 0,

    RenderTextureType_Count
};

enum RenderTextureGPUFormat : uint8_t
{
    RGBA4 = 0,
    RGB5_A1,

    RenderTextureGPUFormat_Count,
};

enum RenderTextureFormat : uint8_t
{
    Unsigned_RGBA8 = 0,
    Unsigned_A1_BGR5,

    RenderTextureFormat_Count,
};

enum ShaderUniformType : uint8_t
{
    Int1 = 0,

    // Large uniforms start (variable size)
    Float1V,

    ShaderUniform_Count,
    ShaderUniform_Invalid = 0xff,

    ShaderUniform_VariableFirst = Float1V,
    ShaderUniform_VariableLast = ShaderUniform_Count - 1,
    ShaderUniform_VariableCount = ShaderUniform_VariableLast - ShaderUniform_VariableFirst + 1,
    ShaderUniform_FixedFirst = Int1,
    ShaderUniform_FixedLast = ShaderUniform_VariableFirst - 1,
    ShaderUniform_FixedCount = ShaderUniform_FixedLast - ShaderUniform_FixedFirst + 1,
};

enum ShaderStage : uint8_t
{
    VertexShader = 0,
    FragmentShader,

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
        RenderTextureType type;
    } texture = { RENDER_TEXTUREHANDLE_INVALID, RenderTextureType::Texture2D };

    ShaderPipeline pipeline;
    RenderStateUniformCache uniformCache;

    struct
    {
        bool enabled;
        BlendFunc func;
    } blend = { false, BlendFunc::SrcAlpha_OneMinusSrcAlpha };

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
    } stencil = { false,           StencilFunc::NeverPass, StencilOp::Keep,
                  StencilOp::Keep, StencilOp::Keep,        0,
                  0xffffffff };

    ColorMask colorMask = ColorMask::ColorMask_All;
    float4 color = g_ColorWhite;
};

struct RenderCmdList
{
    RenderState state;
    char *data = nullptr;
    uint32_t size = 0;
    uint32_t remainingSize = 0;
    bool immediateMode = false;

    RenderCmdList(void *data, uint32_t size, RenderState state, bool immediateMode)
        : state(state)
        , data((char *)data)
        , size(size)
        , remainingSize(size)
        , immediateMode(immediateMode)
    {
    }

    RenderCmdList() = default;
};
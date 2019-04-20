#pragma once

#define NEW_RENDERER_ENABLED

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

// FIXME better vector support
typedef struct
{
    float rgba[4];

    float operator[](size_t i);
} float4;

extern float4 g_ColorWhite;

enum BlendFunc : uint8_t
{
    SrcAlpha_OneMinusSrcAlpha = 0,

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

enum RenderCommandType : uint8_t
{
    Cmd_DrawQuad = 0,
    Cmd_DrawRotatedQuad,
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

enum RenderTextureType : uint8_t
{
    Texture2D = 0,

    RenderTextureType_Count
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

// TODO separate render commands into a new file
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
        uint32_t x,
        uint32_t y,
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

struct RenderStateShaderUniform
{
    uint32_t offset = 0xffffffff;
    // ShaderUniformType type = ShaderUniformType::ShaderUniform_Invalid;
};

struct RenderStateUniformCache
{
    RenderStateShaderUniform uniforms[RENDERSTATE_MAX_SHADER_UNIFORM];
    uint8_t data[RENDERSTATE_SHADER_UNIFORMDATA_SIZE];
    uint32_t dataUsedSize;
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

RenderCmdList
Render_CmdList(void *buffer, uint32_t bufferSize, RenderState state, bool immediateMode = false);
RenderState Render_DefaultState();
void Render_ResetCmdList(RenderCmdList *cmdList, RenderState state);

bool Render_CreateShaderPipeline(
    const char *vertexShaderSource, const char *fragmentShaderSource, ShaderPipeline *pipeline);
bool Render_DestroyShaderPipeline(ShaderPipeline *pipeline);
uint32_t Render_GetUniformId(ShaderPipeline *pipeline, const char *uniform);

bool RenderAdd_SetTexture(RenderCmdList *cmdList, SetTextureCmd *cmd);
bool RenderAdd_DrawQuad(RenderCmdList *cmdList, DrawQuadCmd *cmds, uint32_t cmd_count);
bool RenderAdd_DrawRotatedQuad(
    RenderCmdList *cmdList, DrawRotatedQuadCmd *cmds, uint32_t cmd_count);

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

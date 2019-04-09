#pragma once

#define TEXTUREHANDLE_INVALID (0xffffffff)

typedef uint32_t textureHandle_t;

// FIXME better vector support
typedef struct
{
    float rgba[4];

    float operator[](size_t i)
    {
        assert(i < countof(rgba));
        return rgba[i];
    };
} float4;
extern float4 g_ColorWhite;

enum BlendFunc : uint8_t
{
    SrcAlpha_OneMinusSrcAlpha = 0,

    Count,
    Invalid = 0xff,
};

enum RenderCommandType : uint8_t
{
    Texture = 0,
    RotatedTexture,

    FlushState,

    BlendState,
    DisableBlendState,
    StencilState,
    DisableStencilState
};

enum RenderTextureType : uint8_t
{
    Texture2D = 0,
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

struct TextureCmd
{
    RenderCommandHeader header;
    textureHandle_t texture;
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    float u = 1.f;
    float v = 1.f;
    float4 rgba = g_ColorWhite;

    TextureCmd() = default;

    TextureCmd(
        textureHandle_t texture,
        uint32_t x,
        uint32_t y,
        uint32_t width,
        uint32_t height,
        float u,
        float v,
        float4 rgba)
        : header{ RenderCommandType::Texture }
        , texture(texture)
        , x(x)
        , y(y)
        , width(width)
        , height(height)
        , u(u)
        , v(v)
        , rgba(rgba)
    {
    }
};

struct RotatedTextureCmd : public TextureCmd
{
    float angle;

    RotatedTextureCmd(
        textureHandle_t texture,
        uint32_t x,
        uint32_t y,
        uint32_t width,
        uint32_t height,
        float angle,
        float u = 1.f,
        float v = 1.f,
        float4 rgba = g_ColorWhite)
        : TextureCmd(texture, x, y, width, height, u, v, rgba)
        , angle(angle)
    {
        header.type = RenderCommandType::RotatedTexture;
    }
};

struct BlendStateCmd
{
    RenderCommandHeader header;
    BlendFunc func = BlendFunc{ 0 };

    BlendStateCmd(BlendFunc func)
        : header{ RenderCommandType::BlendState }
        , func(func)
    {
    }
};

struct DisableBlendStateCmd
{
    RenderCommandHeader header;
    DisableBlendStateCmd()
        : header{ RenderCommandType::DisableBlendState }
    {
    }
};

struct StencilStateCmd
{
    RenderCommandHeader header;
    StencilStateCmd()
        : header{ RenderCommandType::StencilState }
    {
    }
};

struct DisableStencilStateCmd
{
    RenderCommandHeader header;
    DisableStencilStateCmd()
        : header{ RenderCommandType::DisableStencilState }
    {
    }
};

struct FlushStateCmd
{
    RenderCommandHeader header;
    FlushStateCmd()
        : header{ RenderCommandType::FlushState }
    {
    }
};

struct RenderState
{
    struct
    {
        textureHandle_t texture;
        RenderTextureType type;
    } texture = { TEXTUREHANDLE_INVALID, RenderTextureType::Texture2D };
    struct
    {
        bool enabled;
        BlendFunc func;
    } blend = { false, BlendFunc::SrcAlpha_OneMinusSrcAlpha };

    struct
    {
        bool enabled;
    } stencil = { false };
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

TextureCmd RenderAdd_TextureCmd(
    textureHandle_t texture,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    float u = 1.f,
    float v = 1.f,
    float4 rgba = g_ColorWhite);

RotatedTextureCmd RenderAdd_RotatedTextureCmd(
    textureHandle_t texture,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    float angle,
    float u = 1.f,
    float v = 1.f,
    float4 rgba = g_ColorWhite);

BlendStateCmd RenderAdd_Blend(BlendFunc func);
StencilStateCmd RenderAdd_Stencil();

bool RenderAdd_Texture(RenderCmdList *cmdList, TextureCmd *textures, uint32_t texture_count);
bool RenderAdd_RotatedTexture(
    RenderCmdList *cmdList, RotatedTextureCmd *textures, uint32_t texture_count);
bool RenderAdd_SetBlend(RenderCmdList *cmdList, BlendStateCmd *state);
bool RenderAdd_DisableBlend(RenderCmdList *cmdList);
bool RenderAdd_SetStencil(RenderCmdList *cmdList, StencilStateCmd *state);
bool RenderAdd_DisableStencil(RenderCmdList *cmdList);
bool RenderAdd_FlushState(RenderCmdList *cmdList);

bool RenderDraw_Execute(RenderCmdList *cmdList);

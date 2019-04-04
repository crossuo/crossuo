#pragma once

enum BlendFunc : uint8_t
{
    SrcAlpha_OneMinusSrcAlpha = 0,
};

enum RenderCommandType : uint8_t
{
    Texture = 0,

    FlushState,

    BlendState,
    StencilState
};

typedef uint32_t textureHandle_t;

struct RenderCommandHeader
{
    RenderCommandType type = RenderCommandType{ 0 };
    RenderCommandHeader(RenderCommandType type)
        : type(type)
    {
    }

    RenderCommandHeader() = default;
};

struct TextureCmd : private RenderCommandHeader
{
    textureHandle_t texture;
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    float u = 0.f;
    float v = 0.f;

    TextureCmd(
        textureHandle_t texture,
        uint32_t x,
        uint32_t y,
        uint32_t width,
        uint32_t height,
        float u,
        float v)
        : RenderCommandHeader(RenderCommandType::Texture)
        , texture(texture)
        , x(x)
        , y(y)
        , width(width)
        , height(height)
        , u(u)
        , v(v)
    {
    }
};

struct BlendStateCmd : private RenderCommandHeader
{
    bool enabled = false;
    BlendFunc func = BlendFunc{ 0 };

    BlendStateCmd(bool enabled, BlendFunc func)
        : RenderCommandHeader(RenderCommandType::BlendState)
        , enabled(enabled)
        , func(func)
    {
    }
};

struct StencilStateCmd : private RenderCommandHeader
{
    bool enabled = false;

    StencilStateCmd(bool enabled)
        : RenderCommandHeader(RenderCommandType::StencilState)
        , enabled(enabled)
    {
    }
};

struct FlushStateCmd : private RenderCommandHeader
{
    FlushStateCmd()
        : RenderCommandHeader(RenderCommandType::FlushState)
    {
    }
};

struct RenderCmdList
{
    char *data = nullptr;
    uint32_t size = 0;
    uint32_t remainingSize = 0;

    RenderCmdList(void *data, uint32_t size)
        : data((char *)data)
        , size(size)
        , remainingSize(size)
    {
    }
};

enum RenderTextureType : uint8_t
{
    Texture2D = 0,
};

#define TEXTUREHANDLE_INVALID (0xffffffff)

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

RenderCmdList Render_CmdList(void *buffer, uint32_t bufferSize);
RenderState Render_DefaultState();
void Render_ResetCmdList(RenderCmdList *cmdList);

TextureCmd RenderAdd_TextureCmd(
    textureHandle_t texture,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    float u,
    float v);
BlendStateCmd RenderAdd_Blend(bool enabled, BlendFunc func);
StencilStateCmd RenderAdd_Stencil(bool enabled);

bool RenderAdd_Texture(RenderCmdList *cmdList, TextureCmd *textures, uint32_t texture_count);
bool RenderAdd_SetBlend(RenderCmdList *cmdList, BlendStateCmd *state);
bool RenderAdd_SetStencil(RenderCmdList *cmdList, StencilStateCmd *state);
bool RenderAdd_FlushState(RenderCmdList *cmdList, RenderState *state);

bool RenderDraw_Execute(RenderCmdList *cmdList, RenderState *state);
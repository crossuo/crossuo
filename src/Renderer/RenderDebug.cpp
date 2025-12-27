// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Everton Fernando Patitucci da Silva

#include "../Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "../Renderer/RenderInternal.h"
#include "../Utility/PerfMarker.h"
#include <external/gfx/gfx.h>
#include <assert.h>
#include <common/utils.h> // countof
#include <common/logging/logging.h>
#include <stdio.h> // snprintf, fprintf, fopen, fclose
#include <string.h>

#define MATCH_CASE_DRAW_DEBUG(type, cmd, state)                                                    \
    case RenderCommandType::Cmd_##type:                                                            \
    {                                                                                              \
        RenderDraw_##type##Debug((type##Cmd *)cmd, state);                                         \
        cmd += sizeof(type##Cmd);                                                                  \
        break;                                                                                     \
    }

#ifdef OGL_DEBUGCONTEXT_ENABLED
#define OGL_DEBUGMSG_SEVERITY_COUNT (3)
#define OGL_DEBUGMSG_TYPE_COUNT (16)
#define OGL_DEBUGMSG_IDS_MAX (16)
#define OGL_DEBUGMSG_INVALIDID (0xffffffff)

enum OpenGLDebugMsgState
{
    OGL_DBGMSG_UNSET = 0,
    OGL_DBGMSG_ENABLED,
    OGL_DBGMSG_DISABLED,
};

struct
{
    OpenGLDebugMsgState assert = OGL_DBGMSG_UNSET;
    OpenGLDebugMsgState log = OGL_DBGMSG_UNSET;
} static s_openglDebugMsgSeverity[OGL_DEBUGMSG_SEVERITY_COUNT];

struct
{
    OpenGLDebugMsgState assert = OGL_DBGMSG_UNSET;
    OpenGLDebugMsgState log = OGL_DBGMSG_UNSET;
} static s_openglDebugMsgType[OGL_DEBUGMSG_TYPE_COUNT];

struct
{
    GLuint id = OGL_DEBUGMSG_INVALIDID;
    OpenGLDebugMsgState assert = OGL_DBGMSG_UNSET;
    OpenGLDebugMsgState log = OGL_DBGMSG_UNSET;
} static s_openglDebugMsgs[OGL_DEBUGMSG_IDS_MAX];

static void EnableOpenGLDebugMsgSeverity(GLenum severity, bool shouldAssert, bool shouldLog)
{
    auto &info = s_openglDebugMsgSeverity[severity % OGL_DEBUGMSG_SEVERITY_COUNT];
    info.assert = shouldAssert ? OGL_DBGMSG_ENABLED : OGL_DBGMSG_DISABLED;
    info.log = shouldLog ? OGL_DBGMSG_ENABLED : OGL_DBGMSG_DISABLED;
}

static void EnableOpenGLDebugMsgType(GLenum type, bool shouldAssert, bool shouldLog)
{
    auto &info = s_openglDebugMsgType[type % OGL_DEBUGMSG_TYPE_COUNT];
    info.assert = shouldAssert ? OGL_DBGMSG_ENABLED : OGL_DBGMSG_DISABLED;
    info.log = shouldLog ? OGL_DBGMSG_ENABLED : OGL_DBGMSG_DISABLED;
}

static void EnableOpenGLMessage(GLuint id, bool shouldAssert, bool shouldLog)
{
    for (auto &msg : s_openglDebugMsgs)
    {
        if (msg.id == OGL_DEBUGMSG_INVALIDID)
        {
            msg.id = id;
            msg.assert = shouldAssert ? OGL_DBGMSG_ENABLED : OGL_DBGMSG_DISABLED;
            msg.log = shouldLog ? OGL_DBGMSG_ENABLED : OGL_DBGMSG_DISABLED;
            return;
        }
    }

    Error(Renderer, "Can't change settings for OpenGL debug message, OGL_DEBUGMSG_IDS_MAX reached");
    assert(false);
}

#if defined(USE_GL2)
static void OGLDebugMsgCallback(
    uint source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar *message,
    const void *userParam)
#endif // #if defined(USE_GL2)
#if defined(USE_GL3)
    static void APIENTRY OGLDebugMsgCallback(
        uint source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar *message,
        void *userParam)
#endif // #if defined(USE_GL2)
{
    (void)source;
    (void)length;
    (void)userParam;

    auto getMsgInfo = [](GLenum sev, GLenum type, GLuint id) {
        auto &infoSeverity = s_openglDebugMsgSeverity[sev % OGL_DEBUGMSG_SEVERITY_COUNT];
        auto &infoType = s_openglDebugMsgType[type % OGL_DEBUGMSG_TYPE_COUNT];
        auto [shouldAssert, shouldLog] = std::tie(infoSeverity.assert, infoSeverity.log);
        if (infoType.assert != OGL_DBGMSG_UNSET)
            shouldAssert = infoType.assert;
        if (infoType.log != OGL_DBGMSG_UNSET)
            shouldLog = infoType.log;

        if (sev == GL_DEBUG_SEVERITY_NOTIFICATION)
        {
            shouldAssert = OGL_DBGMSG_DISABLED;
            shouldLog = OGL_DBGMSG_DISABLED;
        }

        for (auto &ctrl : s_openglDebugMsgs)
        {
            if (ctrl.id == OGL_DEBUGMSG_INVALIDID)
            {
                break;
            }

            if (ctrl.id == id && ctrl.assert != OGL_DBGMSG_UNSET)
            {
                shouldAssert = ctrl.assert;
                shouldLog = ctrl.log;
                break;
            }
        }

        return std::tie(shouldAssert, shouldLog);
    };

    auto [shouldAssert, shouldLog] = getMsgInfo(severity, type, id);
    (void)shouldAssert;
    if (shouldLog == OGL_DBGMSG_ENABLED)
    {
        Info(Renderer, "OpenGL debug message (id %d): %s", id, message);
    }
    assert(shouldAssert == OGL_DBGMSG_DISABLED);
}
#endif // OGL_DEBUGCONTEXT_ENABLED

void SetupOGLDebugMessage()
{
#ifdef OGL_DEBUGCONTEXT_ENABLED
    EnableOpenGLDebugMsgSeverity(GL_DEBUG_SEVERITY_HIGH, true, true);
    EnableOpenGLDebugMsgSeverity(GL_DEBUG_SEVERITY_MEDIUM, false, true);
    EnableOpenGLDebugMsgSeverity(GL_DEBUG_SEVERITY_LOW, false, true);

    EnableOpenGLDebugMsgType(GL_DEBUG_TYPE_ERROR, true, true);
    EnableOpenGLDebugMsgType(GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, true, true);
    EnableOpenGLDebugMsgType(GL_DEBUG_TYPE_OTHER, true, true);
    EnableOpenGLDebugMsgType(GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, false, true);
    EnableOpenGLDebugMsgType(GL_DEBUG_TYPE_PORTABILITY, false, true);
    EnableOpenGLDebugMsgType(GL_DEBUG_TYPE_PERFORMANCE, false, true);
    EnableOpenGLDebugMsgType(GL_DEBUG_TYPE_MARKER, false, false);
    EnableOpenGLDebugMsgType(GL_DEBUG_TYPE_POP_GROUP, false, true);

    EnableOpenGLMessage(OGL_USERPERFMARKERS_ID, false, false);

    // GL error "GL_INVALID_OPERATION in ...":
    // 1) FIXME no shader set when glUniform1iARB is called for g_ShaderDrawMode
    EnableOpenGLMessage(1282, false, true);

    // Usage warning: glClear() called with GL_STENCIL_BUFFER_BIT, but there is no stencil buffer. Operation will have no effect.
    EnableOpenGLMessage(131076, false, false);

    // Texture state usage warning: The texture object (4294967295) bound to texture image unit 0 does not have a defined base level and cannot be used for texture mapping.
    EnableOpenGLMessage(131204, false, true);

    glDebugMessageCallback(OGLDebugMsgCallback, nullptr);
    glDebugMessageControl(
        GL_DONT_CARE, // source
        GL_DONT_CARE, // type
        GL_DONT_CARE, // severity
        0,
        nullptr,
        GL_TRUE);
#endif
}

static const char *BlendFactorAsString(BlendFactor factor)
{
    static const char *s_factorToText[] = {
        "BlendFactor_Zero",
        "BlendFactor_One",
        "BlendFactor_OneMinusSrcAlpha",
        "BlendFactor_OneMinusSrcColor",
        "BlendFactor_SrcColor",
        "BlendFactor_SrcAlpha",
        "BlendFactor_DstColor",
    };

    static_assert(
        countof(s_factorToText) == BlendFactor::BlendFactor_Count, "missing blend factors");

    if (factor == BlendFactor::BlendFactor_Invalid)
    {
        return "Invalid";
    }

    assert(factor < BlendFactor::BlendFactor_Count);
    return s_factorToText[factor];
}

static const char *BlendEquationAsString(BlendEquation equation)
{
    static const char *s_equationToText[] = { "BlendEquation_Add",
                                              "BlendEquation_ReverseSubtract" };

    static_assert(
        countof(s_equationToText) == BlendEquation::BlendEquation_Count, "missing blend equations");

    if (equation == BlendEquation::BlendEquation_Invalid)
    {
        return "Invalid";
    }

    assert(equation < BlendEquation::BlendEquation_Count);
    return s_equationToText[equation];
}

static const char *AlphaTestFuncAsString(AlphaTestFunc func)
{
    static const char *s_funcToText[] = {
        "AlphaTestFunc_NeverPass",
        "AlphaTestFunc_AlwaysPass",
        "AlphaTestFunc_Equal",
        "AlphaTestFunc_Different",
        "AlphaTestFunc_Less",
        "AlphaTestFunc_LessOrEqual",
        "AlphaTestFunc_Greater",
        "AlphaTestFunc_GreaterOrEqual",
    };

    static_assert(
        countof(s_funcToText) == AlphaTestFunc::AlphaTestFunc_Count, "missing alpha test functions");

    if (func == AlphaTestFunc::AlphaTestFunc_Invalid)
    {
        return "Invalid";
    }

    assert(func < AlphaTestFunc::AlphaTestFunc_Count);
    return s_funcToText[func];
}

static const char *StencilFuncAsString(StencilFunc func)
{
    static const char *s_funcToText[] = {
        "StencilFunc_NeverPass",
        "StencilFunc_AlwaysPass",
        "StencilFunc_Less",
        "StencilFunc_LessOrEqual",
        "StencilFunc_Greater",
        "StencilFunc_GreaterOrEqual",
        "StencilFunc_Equal",
        "StencilFunc_Different",
    };

    static_assert(
        countof(s_funcToText) == StencilFunc::StencilFunc_Count, "missing stencil functions");

    if (func == StencilFunc::StencilFunc_Invalid)
    {
        return "Invalid";
    }

    assert(func < StencilFunc::StencilFunc_Count);
    return s_funcToText[func];
}

static const char *StencilOpAsString(StencilOp op)
{
    static const char *s_opToText[] = {
        "StencilOp_Keep",
        "StencilOp_Zero",
        "StencilOp_Replace",
        "StencilOp_IncrementClamp",
        "StencilOp_IncrementWrap",
        "StencilOp_DecrementClamp",
        "StencilOp_DecrementWrap",
        "StencilOp_Invert",
    };

    static_assert(countof(s_opToText) == StencilOp::StencilOp_Count, "missing stencil ops");

    if (op == StencilOp::StencilOp_Invalid)
    {
        return "Invalid";
    }

    assert(op < StencilOp::StencilOp_Count);
    return s_opToText[op];
}

static const char *DepthFuncAsString(DepthFunc func)
{
    static const char *s_funcToText[] = {
        "DepthFunc_NeverPass",
        "DepthFunc_AlwaysPass",
        "DepthFunc_Equal",
        "DepthFunc_Different",
        "DepthFunc_Less",
        "DepthFunc_LessOrEqual",
        "DepthFunc_Greater",
        "DepthFunc_GreaterOrEqual",
    };

    static_assert(
        countof(s_funcToText) == DepthFunc::DepthFunc_Count, "missing depth functions");

    if (func == DepthFunc::DepthFunc_Invalid)
    {
        return "Invalid";
    }

    assert(func < DepthFunc::DepthFunc_Count);
    return s_funcToText[func];
}

static const char *ClearRTMaskAsString(ClearRT mask)
{
    static char buffer[64];
    buffer[0] = '\0';
    if (mask & ClearRT::ClearRT_Color)
        strcat(buffer, "Color");
    if (mask & ClearRT::ClearRT_Depth)
        strcat(buffer, buffer[0] ? "|Depth" : "Depth");
    if (mask & ClearRT::ClearRT_Stencil)
        strcat(buffer, buffer[0] ? "|Stencil" : "Stencil");
    if (buffer[0] == '\0')
        strcpy(buffer, "None");
    return buffer;
}

const char *ShaderUniformTypeAsString(ShaderUniformType type)
{
    static const char *s_uniformTypeAsStr[] = {
        "Int1",    // ShaderUniformType::ShaderUniformType_Int1
        "Float1V", // ShaderUniformType::ShaderUniformType_Float1V
    };
    static_assert(
        countof(s_uniformTypeAsStr) == ShaderUniformType::ShaderUniformType_Count,
        "missing shader uniform types");

    return s_uniformTypeAsStr[type];
}

void ShaderUniformValueAsString(ShaderUniformType type, void *value, char *str, size_t str_capacity)
{
    if (type >= ShaderUniformType::ShaderUniformType_FixedFirst &&
        type <= ShaderUniformType::ShaderUniformType_FixedLast)
    {
        switch (type)
        {
            case ShaderUniformType::ShaderUniformType_Int1:
                snprintf(str, str_capacity, "%d", *(int *)value);
                break;

            case ShaderUniformType::ShaderUniformType_Invalid:
            case ShaderUniformType::ShaderUniformType_Count:
            // Large uniforms
            case ShaderUniformType::ShaderUniformType_Float1V:
            default:
                assert(false);
                break;
        }
    }
    else
    {
        assert(
            type >= ShaderUniformType::ShaderUniformType_VariableFirst &&
            type <= ShaderUniformType::ShaderUniformType_VariableLast);
        switch (type)
        {
            case ShaderUniformType::ShaderUniformType_Float1V:
            {
                snprintf(str, str_capacity, "%f, ...", *(float *)value);
                break;
            }

            case ShaderUniformType::ShaderUniformType_Invalid:
            case ShaderUniformType::ShaderUniformType_Count:
            // Small uniform
            case ShaderUniformType::ShaderUniformType_Int1:
            default:
                assert(false);
                break;
        }
    }
}

static const char *TextureTypeAsString(TextureType type)
{
    static const char *s_textureTypeAsStr[] = {
        "Texture2D",          // Texture2D
        "Texture2D_Mipmapped" // Texture2D_Mipmapped
    };

    static_assert(
        countof(s_textureTypeAsStr) == TextureType::TextureType_Count, "missing texture types");

    assert(type < TextureType::TextureType_Count);
    return s_textureTypeAsStr[type];
}

// Frame dump system
static uint32_t s_frameCount = 0;
static bool s_dumpEnabled = false;
static bool s_dumpInProgress = false;
static FILE *s_dumpFile = nullptr;

void RenderDebug_EnableDump()
{
    if (!s_dumpEnabled)
    {
        s_dumpEnabled = true;
        s_dumpInProgress = false;
        Info(Renderer, "Render dump enabled for next frame");
    }
}

void RenderDebug_StartFrame()
{
    s_frameCount++;

    if (s_dumpEnabled && !s_dumpInProgress)
    {
        // Start dumping at the beginning of this frame
        char filename[32];
        snprintf(filename, sizeof(filename), "frame%u.log", s_frameCount);
        s_dumpFile = fopen(filename, "w");
        if (s_dumpFile)
        {
            s_dumpInProgress = true;
            fprintf(s_dumpFile, "=== Frame %u Render Command Dump ===\n", s_frameCount);
        }
        else
        {
            Error(Renderer, "Failed to open dump file: %s", filename);
            s_dumpEnabled = false;
        }
    }
}

void RenderDebug_EndFrame()
{
    if (s_dumpInProgress && s_dumpFile)
    {
        fprintf(s_dumpFile, "=== End of Frame %u ===\n", s_frameCount);
        fclose(s_dumpFile);
        s_dumpFile = nullptr;
        Info(Renderer, "Render dump written to frame%u.log", s_frameCount);
    }

    // Disable dumping after one frame
    s_dumpInProgress = false;
    s_dumpEnabled = false;
}

static void DumpInfo(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), format, args);

    if (s_dumpFile)
    {
        fprintf(s_dumpFile, "%s\n", buffer);
    }
    else
    {
        Info(Renderer, "%s", buffer);
    }

    va_end(args);
}

void RenderDraw_DrawQuadDebug(DrawQuadCmd *cmd, RenderState *state)
{
    DumpInfo(
        "DrawQuadCmd: texture: %d - x: %d - y: %d - width: %d - height: %d - u: %f - v: %f - mirrored: %s | alphaTest: %s, alphaRef: %f",
        cmd->texture,
        cmd->x,
        cmd->y,
        cmd->width,
        cmd->height,
        cmd->u,
        cmd->v,
        cmd->mirrored ? "true" : "false",
        state->alphaTest.enabled ? "true" : "false",
        state->alphaTest.alphaRef);
}

void RenderDraw_DrawRotatedQuadDebug(DrawRotatedQuadCmd *cmd, RenderState *state)
{
    DumpInfo(
        "RotatedTextureCmd: texture: %d - x: %d - y: %d - width: %d - height: %d - angle - %f - u: %f - v: %f - mirrored: %s | alphaTest: %s, alphaRef: %f",
        cmd->texture,
        cmd->x,
        cmd->y,
        cmd->width,
        cmd->height,
        cmd->angle,
        cmd->u,
        cmd->v,
        cmd->mirrored ? "true" : "false",
        state->alphaTest.enabled ? "true" : "false",
        state->alphaTest.alphaRef);
}

void RenderDraw_FlushStateDebug(FlushStateCmd *, RenderState *)
{
    DumpInfo("FlushStateCmd");
}

void RenderDraw_SetTextureDebug(SetTextureCmd *cmd, RenderState *)
{
    auto typeAsStr = TextureTypeAsString(cmd->type);
    DumpInfo("SetTextureCmd: texture: %d - type: %s", cmd->texture, typeAsStr);
}

void RenderDraw_BlendStateDebug(BlendStateCmd *cmd, RenderState *)
{
    DumpInfo(
        "BlendStateCmd: src: %s + dst: %s - equation: %s",
        BlendFactorAsString(cmd->src),
        BlendFactorAsString(cmd->dst),
        BlendEquationAsString(cmd->equation));
}

void RenderDraw_DisableBlendStateDebug(DisableBlendStateCmd *, RenderState *)
{
    DumpInfo("DisableBlendStateCmd");
}

void RenderDraw_StencilStateDebug(StencilStateCmd *cmd, RenderState *)
{
    DumpInfo(
        "StencilStateCmd: func: %s - stencilFail: %s - depthFail: %s - bothFail: %s - ref: %u - mask: 0x%x",
        StencilFuncAsString(cmd->func),
        StencilOpAsString(cmd->stencilFail),
        StencilOpAsString(cmd->depthFail),
        StencilOpAsString(cmd->bothFail),
        cmd->ref,
        cmd->mask);
}

void RenderDraw_DisableStencilStateDebug(DisableStencilStateCmd *, RenderState *)
{
    DumpInfo("DisableStencilStateCmd");
}

void RenderDraw_EnableStencilStateDebug(EnableStencilStateCmd *, RenderState *)
{
    DumpInfo("EnableStencilStateCmd");
}

void RenderDraw_SetColorMaskDebug(SetColorMaskCmd *cmd, RenderState *)
{
    DumpInfo(
        "SetColorMaskCmd: mask: %s|%s|%s|%s",
        cmd->mask & ColorMask::ColorMask_Red ? "Red" : "",
        cmd->mask & ColorMask::ColorMask_Green ? "Green" : "",
        cmd->mask & ColorMask::ColorMask_Blue ? "Blue" : "",
        cmd->mask & ColorMask::ColorMask_Alpha ? "Alpha" : "");
}

void RenderDraw_SetColorDebug(SetColorCmd *cmd, RenderState *)
{
    DumpInfo(
        "SetColorCmd: rgba: (%f, %f, %f, %f)",
        cmd->color[0],
        cmd->color[1],
        cmd->color[2],
        cmd->color[3]);
}

void RenderDraw_ShaderUniformDebug(ShaderUniformCmd *cmd, RenderState *state)
{
    assert(
        cmd->uniform_type >= ShaderUniformType::ShaderUniformType_FixedFirst &&
        cmd->uniform_type <= ShaderUniformType::ShaderUniformType_FixedLast);

    auto typeAsStr = ShaderUniformTypeAsString(cmd->uniform_type);
    char valueAsStr[64] = "<invalid>";
    ShaderUniformValueAsString(
        cmd->uniform_type, &cmd->value.data[0], valueAsStr, countof(valueAsStr));

    auto pipelineValid = state->pipeline.program != RENDER_SHADERPROGRAM_INVALID;
    assert((pipelineValid && cmd->id < countof(state->pipeline.uniforms)) || !pipelineValid);
    DumpInfo(
        "ShaderUniformCmd: id: %d - location: %d - value: %s - type: %s",
        pipelineValid ? cmd->id : RENDER_SHADERUNIFORMID_INVALID,
        pipelineValid ? state->pipeline.uniforms[cmd->id].location :
                        RENDER_SHADERUNIFORMLOC_INVALID,
        valueAsStr,
        typeAsStr);
}

void RenderDraw_ShaderLargeUniformDebug(ShaderLargeUniformCmd *cmd, RenderState *state)
{
    assert(
        cmd->uniform_type >= ShaderUniformType::ShaderUniformType_VariableFirst &&
        cmd->uniform_type <= ShaderUniformType::ShaderUniformType_VariableLast);

    auto typeAsStr = ShaderUniformTypeAsString(cmd->uniform_type);
    char valueAsStr[64] = "<invalid>";
    assert(cmd->count == 1); // TODO
    ShaderUniformValueAsString(cmd->uniform_type, cmd->value, valueAsStr, countof(valueAsStr));

    auto pipelineValid = state->pipeline.program != RENDER_SHADERPROGRAM_INVALID;
    assert((pipelineValid && cmd->id < countof(state->pipeline.uniforms)) || !pipelineValid);
    DumpInfo(
        "ShaderLargeUniformCmd: id: %d - location: %d - count: %d - values: %s - type: %s",
        pipelineValid ? cmd->id : RENDER_SHADERUNIFORMID_INVALID,
        pipelineValid ? state->pipeline.uniforms[cmd->id].location :
                        RENDER_SHADERUNIFORMLOC_INVALID,
        cmd->count,
        valueAsStr,
        typeAsStr);
}

void RenderDraw_ShaderPipelineDebug(ShaderPipelineCmd *cmd, RenderState *)
{
    auto pipelineValid = cmd->pipeline != nullptr;
    DumpInfo(
        "ShaderPipelineCmd: pipeline: %p - program: %d - uniformCount: %u",
        cmd->pipeline,
        pipelineValid ? cmd->pipeline->program : RENDER_SHADERPROGRAM_INVALID,
        pipelineValid ? cmd->pipeline->uniformCount : 0);
}

void RenderDraw_DisableShaderPipelineDebug(DisableShaderPipelineCmd *, RenderState *)
{
    DumpInfo("DisableShaderPipelineCmd");
}

void RenderDraw_DrawCharacterSittingDebug(DrawCharacterSittingCmd *cmd, RenderState *)
{
    DumpInfo(
        "DrawCharacterSittingCmd: texture: %d - x: %d - y: %d - width: %d - height: %d - h3mod: %f - h6mod: %f - h9mod: %f - mirror: %s",
        cmd->texture,
        cmd->x,
        cmd->y,
        cmd->width,
        cmd->height,
        cmd->h3mod,
        cmd->h6mod,
        cmd->h9mod,
        cmd->mirror ? "true" : "false");
}

void RenderDraw_DrawLandTileDebug(DrawLandTileCmd *cmd, RenderState *)
{
    DumpInfo(
        "DrawLandTileCmd: texture: %d - x: %d - y: %d - rect: {x:%d y:%d w:%d h:%d} - drawMode: %d",
        cmd->texture,
        cmd->x,
        cmd->y,
        cmd->rect.x,
        cmd->rect.y,
        cmd->rect.w,
        cmd->rect.h,
        cmd->drawMode);
}

void RenderDraw_DrawShadowDebug(DrawShadowCmd *cmd, RenderState *)
{
    DumpInfo(
        "DrawShadowCmd: texture: %d - x: %d - y: %d - width: %d - height: %d - uniformId: %d - uniformValue: %d - mirror: %s - restoreBlendFunc: %s",
        cmd->texture,
        cmd->x,
        cmd->y,
        cmd->width,
        cmd->height,
        cmd->uniformId,
        cmd->uniformValue,
        cmd->mirror ? "true" : "false",
        cmd->restoreBlendFunc ? "true" : "false");
}

void RenderDraw_DrawCircleDebug(DrawCircleCmd *cmd, RenderState *state)
{
    DumpInfo(
        "DrawCircleCmd: x: %d - y: %d - radius: %f - gradientMode: %d | alphaTest: %s, alphaRef: %f",
        cmd->x,
        cmd->y,
        cmd->radius,
        cmd->gradientMode,
        state->alphaTest.enabled ? "true" : "false",
        state->alphaTest.alphaRef);
}

void RenderDraw_DrawUntexturedQuadDebug(DrawUntexturedQuadCmd *cmd, RenderState *state)
{
    DumpInfo(
        "DrawUntexturedQuadCmd: x: %d - y: %d - width: %d - height: %d - rgba: (%f, %f, %f, %f) | alphaTest: %s, alphaRef: %f",
        cmd->x,
        cmd->y,
        cmd->width,
        cmd->height,
        cmd->color[0],
        cmd->color[1],
        cmd->color[2],
        cmd->color[3],
        state->alphaTest.enabled ? "true" : "false",
        state->alphaTest.alphaRef);
}

void RenderDraw_DrawLineDebug(DrawLineCmd *cmd, RenderState *state)
{
    DumpInfo(
        "DrawLineCmd: x0: %d - y0: %d - x1: %d - y1: %d - rgba: (%f, %f, %f, %f) | alphaTest: %s, alphaRef: %f",
        cmd->x0,
        cmd->y0,
        cmd->x1,
        cmd->y1,
        cmd->color[0],
        cmd->color[1],
        cmd->color[2],
        cmd->color[3],
        state->alphaTest.enabled ? "true" : "false",
        state->alphaTest.alphaRef);
}

void RenderDraw_ClearRTDebug(ClearRTCmd *cmd, RenderState *)
{
    DumpInfo("ClearRTCmd: mask: %s", ClearRTMaskAsString(cmd->clearMask));
}

void RenderDraw_SetFrameBufferDebug(SetFrameBufferCmd *cmd, RenderState *)
{
    DumpInfo("SetFrameBufferCmd: frameBuffer: %u", cmd->frameBuffer);
}

void RenderDraw_AlphaTestDebug(AlphaTestCmd *cmd, RenderState *)
{
    DumpInfo(
        "AlphaTestCmd: func: %s - ref: %f",
        AlphaTestFuncAsString(cmd->func),
        cmd->ref);
}

void RenderDraw_DisableAlphaTestDebug(DisableAlphaTestCmd *, RenderState *)
{
    DumpInfo("DisableAlphaTestCmd");
}

void RenderDraw_DepthStateDebug(DepthStateCmd *cmd, RenderState *)
{
    DumpInfo("DepthStateCmd: func: %s", DepthFuncAsString(cmd->func));
}

void RenderDraw_DisableDepthStateDebug(DisableDepthStateCmd *, RenderState *)
{
    DumpInfo("DisableDepthStateCmd");
}

void RenderDraw_EnableDepthStateDebug(EnableDepthStateCmd *, RenderState *)
{
    DumpInfo("EnableDepthStateCmd");
}

void RenderDraw_SetClearColorDebug(SetClearColorCmd *cmd, RenderState *)
{
    DumpInfo(
        "SetClearColorCmd: rgba: (%f, %f, %f, %f)",
        cmd->color[0],
        cmd->color[1],
        cmd->color[2],
        cmd->color[3]);
}

void RenderDraw_SetViewParamsDebug(SetViewParamsCmd *cmd, RenderState *)
{
    DumpInfo(
        "SetViewParamsCmd: scene: %d,%d %dx%d - window: %dx%d - camera: %d,%d - scale: %f - proj_flipped_y: %s",
        cmd->scene_x,
        cmd->scene_y,
        cmd->scene_width,
        cmd->scene_height,
        cmd->window_width,
        cmd->window_height,
        cmd->camera_nearZ,
        cmd->camera_farZ,
        cmd->scene_scale,
        cmd->proj_flipped_y ? "true" : "false");
}

void RenderDraw_SetModelViewTranslationDebug(SetModelViewTranslationCmd *cmd, RenderState *)
{
    DumpInfo(
        "SetModelViewTranslationCmd: pos: (%f, %f, %f)",
        cmd->pos[0],
        cmd->pos[1],
        cmd->pos[2]);
}

void RenderDraw_SetScissorDebug(SetScissorCmd *cmd, RenderState *)
{
    DumpInfo(
        "SetScissorCmd: x: %d - y: %d - width: %u - height: %u",
        cmd->x,
        cmd->y,
        cmd->width,
        cmd->height);
}

void RenderDraw_DisableScissorDebug(DisableScissorCmd *, RenderState *)
{
    DumpInfo("DisableScissorCmd");
}

void RenderDraw_GetFrameBufferPixelsDebug(GetFrameBufferPixelsCmd *cmd, RenderState *)
{
    DumpInfo(
        "GetFrameBufferPixelsCmd: x: %d - y: %d - width: %u - height: %u - window: %ux%u - data: %p - dataSize: %zu",
        cmd->x,
        cmd->y,
        cmd->width,
        cmd->height,
        cmd->window_width,
        cmd->window_height,
        cmd->data,
        cmd->dataSize);
}

void RenderDraw_DumpCmdList(RenderCmdList *cmdList)
{
    DumpInfo(
        "Dumping cmd list %p, data %p, capacity %dkB, free size %dkB, immediate? %s",
        cmdList,
        cmdList->data,
        cmdList->size,
        cmdList->remainingSize,
        cmdList->immediateMode ? "yes" : "no");

    char *cmd = cmdList->data;
    uint32_t remainingCmdSize = cmdList->size - cmdList->remainingSize;
    char *listEnd = cmd + remainingCmdSize;

    while (cmd < listEnd)
    {
        RenderCommandType type = *(RenderCommandType *)cmd;
        cmd += sizeof(type);
        switch (type)
        {
            MATCH_CASE_DRAW_DEBUG(DrawQuad, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DrawRotatedQuad, cmd, &cmdList->state)

            MATCH_CASE_DRAW_DEBUG(DrawCharacterSitting, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DrawLandTile, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DrawShadow, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DrawCircle, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DrawUntexturedQuad, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DrawLine, cmd, &cmdList->state)

            MATCH_CASE_DRAW_DEBUG(FlushState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(SetTexture, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(SetFrameBuffer, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(AlphaTest, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DisableAlphaTest, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(BlendState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DisableBlendState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(StencilState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DisableStencilState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(EnableStencilState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DepthState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DisableDepthState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(EnableDepthState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(SetColorMask, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(SetColor, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(SetClearColor, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(SetViewParams, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(SetModelViewTranslation, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(SetScissor, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DisableScissor, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(ClearRT, cmd, &cmdList->state)

            MATCH_CASE_DRAW_DEBUG(ShaderUniform, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(ShaderLargeUniform, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(ShaderPipeline, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DisableShaderPipeline, cmd, &cmdList->state)

            MATCH_CASE_DRAW_DEBUG(GetFrameBufferPixels, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(PushDebugMarker, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(PopDebugMarker, cmd, &cmdList->state)

            case RenderCommandType_Invalid:
                DumpInfo("Invalid type.");
                assert(false);
                break;
            default:
                DumpInfo("Unknown command type: %d", type);
                assert(false);
                break;
        }
    }
}

void RenderDebug_ProcessFrame(RenderCmdList *cmdList)
{
    RenderDebug_StartFrame();
    if (s_dumpInProgress && cmdList)
    {
        RenderDraw_DumpCmdList(cmdList);
    }
    RenderDebug_EndFrame();
}

bool RenderDraw_PushDebugMarker(const PushDebugMarkerCmd &cmd, RenderState *state)
{
    (void)state;
#if defined(NEW_RENDERER_ENABLED) && defined(USE_GL3)
    if (cmd.label)
    {
        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, OGL_USERPERFMARKERS_ID, -1, cmd.label);
    }
#else
    (void)cmd;
#endif
    return true;
}

bool RenderDraw_PopDebugMarker(const PopDebugMarkerCmd &cmd, RenderState *state)
{
    (void)state;
#if defined(NEW_RENDERER_ENABLED) && defined(USE_GL3)
    glPopDebugGroup();
#else
    (void)cmd;
#endif
    return true;
}

void RenderDraw_PushDebugMarkerDebug(const PushDebugMarkerCmd *cmd, RenderState *state)
{
    (void)state;
    DumpInfo("PushDebugMarker: %s", cmd->label ? cmd->label : "(null)");
}

void RenderDraw_PopDebugMarkerDebug(const PopDebugMarkerCmd *cmd, RenderState *state)
{
    (void)state;
    (void)cmd;
    DumpInfo("PopDebugMarker");
}

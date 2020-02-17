#include "../Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "../Renderer/RenderInternal.h"
#include "../Utility/PerfMarker.h"
#include "../GLEngine/GLHeaders.h"
#include <assert.h>
#include <common/utils.h> // countof
#include <common/logging/logging.h>

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
} static s_openglDebugMsgType[OGL_DEBUGMSG_SEVERITY_COUNT];

struct
{
    GLuint id = OGL_DEBUGMSG_INVALIDID;
    OpenGLDebugMsgState assert = OGL_DBGMSG_UNSET;
    OpenGLDebugMsgState log = OGL_DBGMSG_UNSET;
} static s_openglDebugMsgs[OGL_DEBUGMSG_IDS_MAX];

static void EnableOpenGLDebugMsgSeverity(GLenum severity, bool assert, bool log)
{
    auto &info = s_openglDebugMsgSeverity[severity % OGL_DEBUGMSG_SEVERITY_COUNT];
    info.assert = assert ? OGL_DBGMSG_ENABLED : OGL_DBGMSG_DISABLED;
    info.log = log ? OGL_DBGMSG_ENABLED : OGL_DBGMSG_DISABLED;
}

static void EnableOpenGLDebugMsgType(GLenum type, bool assert, bool log)
{
    auto &info = s_openglDebugMsgType[type % OGL_DEBUGMSG_TYPE_COUNT];
    info.assert = assert ? OGL_DBGMSG_ENABLED : OGL_DBGMSG_DISABLED;
    info.log = log ? OGL_DBGMSG_ENABLED : OGL_DBGMSG_DISABLED;
}

static void EnableOpenGLMessage(GLuint id, bool assert, bool log)
{
    for (auto &msg : s_openglDebugMsgs)
    {
        if (msg.id == OGL_DEBUGMSG_INVALIDID)
        {
            msg.id = id;
            msg.assert = assert ? OGL_DBGMSG_ENABLED : OGL_DBGMSG_DISABLED;
            msg.log = log ? OGL_DBGMSG_ENABLED : OGL_DBGMSG_DISABLED;
            return;
        }
    }

    Error(Renderer, "Can't change settings for OpenGL debug message, OGL_DEBUGMSG_IDS_MAX reached");
    assert(false);
}

static void APIENTRY OGLDebugMsgCallback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar *message,
    const void *userParam)
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
                snprintf(str, str_capacity, "%d", *(GLint *)value);
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
                snprintf(str, str_capacity, "%f, ...", *(GLfloat *)value);
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

void RenderDraw_DrawQuadDebug(DrawQuadCmd *cmd, RenderState *)
{
    Info(
        Renderer,
        "DrawQuadCmd: texture: %d - x: %d - y: %d - width: %d - height: %d - u: %f - v: %f - rgba: [%f, %f, %f, %f] - mirrored: %s\n",
        cmd->texture,
        cmd->x,
        cmd->y,
        cmd->width,
        cmd->height,
        cmd->u,
        cmd->v,
        cmd->rgba[0],
        cmd->rgba[1],
        cmd->rgba[2],
        cmd->rgba[3],
        cmd->mirrored ? "true" : "false");
}

void RenderDraw_DrawRotatedQuadDebug(DrawRotatedQuadCmd *cmd, RenderState *)
{
    Info(
        Renderer,
        "RotatedTextureCmd: texture: %d - x: %d - y: %d - width: %d - height: %d - angle - %f - u: %f - v: %f - rgba: [%f, %f, %f, %f] - mirrored: %s\n",
        cmd->texture,
        cmd->x,
        cmd->y,
        cmd->width,
        cmd->height,
        cmd->angle,
        cmd->u,
        cmd->v,
        cmd->rgba[0],
        cmd->rgba[1],
        cmd->rgba[2],
        cmd->rgba[3],
        cmd->mirrored ? "true" : "false");
}

void RenderDraw_FlushStateDebug(FlushStateCmd *, RenderState *)
{
    Info(Renderer, "FlushStateCmd\n");
    assert(false); // TODO
}

void RenderDraw_SetTextureDebug(SetTextureCmd *cmd, RenderState *)
{
    auto typeAsStr = TextureTypeAsString(cmd->type);
    Info(Renderer, "SetTextureCmd: texture: %d - type: %s\n", cmd->texture, typeAsStr);
}

void RenderDraw_BlendStateDebug(BlendStateCmd *cmd, RenderState *)
{
    Info(
        Renderer,
        "BlendStateCmd: src: %s + dst: %s - equation: %s\n",
        BlendFactorAsString(cmd->src),
        BlendFactorAsString(cmd->dst),
        BlendEquationAsString(cmd->equation));
}

void RenderDraw_DisableBlendStateDebug(DisableBlendStateCmd *, RenderState *)
{
    Info(Renderer, "DisableBlendStateCmd\n");
    assert(false); // TODO
}

void RenderDraw_StencilStateDebug(StencilStateCmd *, RenderState *)
{
    Info(Renderer, "StencilStateCmd\n");
    assert(false); // TODO
}

void RenderDraw_DisableStencilStateDebug(DisableStencilStateCmd *, RenderState *)
{
    Info(Renderer, "DisableStencilStateCmd\n");
    assert(false); // TODO
}

void RenderDraw_EnableStencilStateDebug(EnableStencilStateCmd *, RenderState *)
{
    Info(Renderer, "EnableStencilStateCmd\n");
    assert(false); // TODO
}

void RenderDraw_SetColorMaskDebug(SetColorMaskCmd *cmd, RenderState *)
{
    Info(
        Renderer,
        "SetColorMaskCmd: mask: %s|%s|%s|%s\n",
        cmd->mask & ColorMask::ColorMask_Red ? "Red" : "",
        cmd->mask & ColorMask::ColorMask_Green ? "Green" : "",
        cmd->mask & ColorMask::ColorMask_Blue ? "Blue" : "",
        cmd->mask & ColorMask::ColorMask_Alpha ? "Alpha" : "");
}

void RenderDraw_SetColorDebug(SetColorCmd *cmd, RenderState *)
{
    Info(
        Renderer,
        "SetColorCmd: rgba: (%f, %f, %f, %f)\n",
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
    Info(
        Renderer,
        "ShaderUniformCmd: id: %d - location: %d - value: %s - type: %s\n",
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
    Info(
        Renderer,
        "ShaderLargeUniformCmd: id: %d - location: %d - count: %d - values: %s - type: %s\n",
        pipelineValid ? cmd->id : RENDER_SHADERUNIFORMID_INVALID,
        pipelineValid ? state->pipeline.uniforms[cmd->id].location :
                        RENDER_SHADERUNIFORMLOC_INVALID,
        cmd->count,
        valueAsStr,
        typeAsStr);
}

void RenderDraw_ShaderPipelineDebug(ShaderPipelineCmd *, RenderState *)
{
    assert(false); // TODO
}

void RenderDraw_DisableShaderPipelineDebug(DisableShaderPipelineCmd *, RenderState *)
{
    assert(false); // TODO
}

void RenderDraw_DumpCmdList(RenderCmdList *cmdList)
{
    Info(
        Renderer,
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
        RenderCommandHeader &cmdHeader = *(RenderCommandHeader *)cmd;
        switch (cmdHeader.type)
        {
            MATCH_CASE_DRAW_DEBUG(DrawQuad, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DrawRotatedQuad, cmd, &cmdList->state)

            MATCH_CASE_DRAW_DEBUG(FlushState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(SetTexture, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(BlendState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DisableBlendState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(StencilState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DisableStencilState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(EnableStencilState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(SetColorMask, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(SetColor, cmd, &cmdList->state)

            MATCH_CASE_DRAW_DEBUG(ShaderUniform, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(ShaderLargeUniform, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(ShaderPipeline, cmd, &cmdList->state);
            MATCH_CASE_DRAW_DEBUG(DisableShaderPipeline, cmd, &cmdList->state);

            case Cmd_DrawCharacterSitting:
            case Cmd_DrawLandTile:
            case Cmd_DrawShadow:
            case Cmd_DrawCircle:
            case Cmd_DrawUntexturedQuad:
            case Cmd_DrawLine:
            case Cmd_ClearRT:
            case Cmd_SetFrameBuffer:
            case Cmd_AlphaTest:
            case Cmd_DisableAlphaTest:
            case Cmd_DepthState:
            case Cmd_DisableDepthState:
            case Cmd_EnableDepthState:
            case Cmd_SetClearColor:
            case Cmd_SetViewParams:
            case Cmd_SetModelViewTranslation:
            case Cmd_SetScissor:
            case Cmd_DisableScissor:
            case Cmd_GetFrameBufferPixels:
            case RenderCommandType_Invalid:
                // TODO
            default:
                assert(false);
                break;
        }
    }
}
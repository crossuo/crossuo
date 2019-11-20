#include "../Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "../Renderer/RenderInternal.h"
#include "../Utility/PerfMarker.h"
#include "../GLEngine/GLHeaders.h"
#include <assert.h>
#define XUO_M_PI 3.14159265358979323846264338327950288

#define MATCH_CASE_DRAW(type, cmd, state)                                                          \
    case RenderCommandType::Cmd_##type:                                                            \
    {                                                                                              \
        RenderDraw_##type(*(const type##Cmd *)cmd, state);                                         \
        cmd += sizeof(type##Cmd);                                                                  \
        break;                                                                                     \
    }

bool RenderDraw_SetTexture(const SetTextureCmd &cmd, RenderState *state)
{
    return RenderState_SetTexture(state, cmd.type, cmd.texture);
}

bool RenderDraw_SetFrameBuffer(const SetFrameBufferCmd &cmd, RenderState *state)
{
    return RenderState_SetFrameBuffer(state, cmd.frameBuffer);
}

bool RenderDraw_DrawQuad(const DrawQuadCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);
    RenderState_SetTexture(state, TextureType::TextureType_Texture2D, cmd.texture);
    RenderState_SetColor(state, cmd.rgba);
    glTranslatef((GLfloat)cmd.x, (GLfloat)cmd.y, 0.0f);
    glBegin(GL_TRIANGLE_STRIP);

    if (!cmd.mirrored)
    {
        glTexCoord2f(0.0f, cmd.v);
        glVertex2i(0, GLint(cmd.height));
        glTexCoord2f(cmd.u, cmd.v);
        glVertex2i(cmd.width, GLint(cmd.height));
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i(0, 0);
        glTexCoord2f(cmd.u, 0.0f);
        glVertex2i(cmd.width, 0);
    }
    else
    {
        glTexCoord2f(0.0f, cmd.v);
        glVertex2i(cmd.width, GLint(cmd.height));
        glTexCoord2f(cmd.u, cmd.v);
        glVertex2i(0, GLint(cmd.height));
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i(cmd.width, 0);
        glTexCoord2f(cmd.u, 0.0f);
        glVertex2i(0, 0);
    }

    glEnd();
    glTranslatef(-(GLfloat)cmd.x, -(GLfloat)cmd.y, 0.0f);

    return true;
}

bool RenderDraw_DrawRotatedQuad(const DrawRotatedQuadCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);

    RenderState_SetTexture(state, TextureType::TextureType_Texture2D, cmd.texture);
    RenderState_SetColor(state, cmd.rgba);

    glTranslatef((GLfloat)cmd.x, (GLfloat)cmd.y, 0.0f);
    glRotatef(cmd.angle, 0.0f, 0.0f, 1.0f);

    glBegin(GL_TRIANGLE_STRIP);

    if (!cmd.mirrored)
    {
        glTexCoord2f(0.0f, cmd.v);
        glVertex2i(0, cmd.height);
        glTexCoord2f(cmd.u, cmd.v);
        glVertex2i(cmd.width, cmd.height);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i(0, 0);
        glTexCoord2f(cmd.u, 0.0f);
        glVertex2i(cmd.width, 0);
    }
    else
    {
        glTexCoord2f(0.0f, cmd.v);
        glVertex2i(cmd.width, cmd.height);
        glTexCoord2f(cmd.u, cmd.v);
        glVertex2i(0, cmd.height);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i(cmd.width, 0);
        glTexCoord2f(cmd.u, 0.0f);
        glVertex2i(0, 0);
    }

    glEnd();

    glTranslatef(-(GLfloat)cmd.x, -(GLfloat)cmd.y, 0.0f);
    glRotatef(cmd.angle, 0.0f, 0.0f, -1.0f);

    return true;
}

bool RenderDraw_DrawCharacterSitting(const DrawCharacterSittingCmd &cmd, RenderState *state)
{
    static const auto s_sittingCharacterOffset = 8.f;

    ScopedPerfMarker(__FUNCTION__);

    RenderState_SetTexture(state, TextureType::TextureType_Texture2D, cmd.texture);

    auto x = (GLfloat)cmd.x;
    auto y = (GLfloat)cmd.y;
    float width = (float)cmd.width;
    float height = (float)cmd.height;
    float h03 = height * cmd.h3mod;
    float h06 = height * cmd.h6mod;
    float h09 = height * cmd.h9mod;
    float widthOffset = (float)(width + s_sittingCharacterOffset);

    glTranslatef(x, y, 0.0f);

    glBegin(GL_TRIANGLE_STRIP);

    if (cmd.mirror)
    {
        // TODO this won't end well... add a isAlmostZeroF(float val)
        if (cmd.h3mod != 0.0f)
        {
            glTexCoord2f(0.0f, 0.0f);
            glVertex2f(width, 0);
            glTexCoord2f(1.0f, 0.0f);
            glVertex2f(0, 0);
            glTexCoord2f(0.0f, cmd.h3mod);
            glVertex2f(width, h03);
            glTexCoord2f(1.0f, cmd.h3mod);
            glVertex2f(0, h03);
        }

        if (cmd.h6mod != 0.0f)
        {
            if (cmd.h3mod == 0.0f)
            {
                glTexCoord2f(0.0f, 0.0f);
                glVertex2f(width, 0);
                glTexCoord2f(1.0f, 0.0f);
                glVertex2f(0, 0);
            }

            glTexCoord2f(0.0f, cmd.h6mod);
            glVertex2f(widthOffset, h06);
            glTexCoord2f(1.0f, cmd.h6mod);
            glVertex2f(s_sittingCharacterOffset, h06);
        }

        if (cmd.h9mod != 0.0f)
        {
            if (cmd.h6mod == 0.0f)
            {
                glTexCoord2f(0.0f, 0.0f);
                glVertex2f(widthOffset, 0);
                glTexCoord2f(1.0f, 0.0f);
                glVertex2f(s_sittingCharacterOffset, 0);
            }

            glTexCoord2f(0.0f, 1.0f);
            glVertex2f(widthOffset, h09);
            glTexCoord2f(1.0f, 1.0f);
            glVertex2f(s_sittingCharacterOffset, h09);
        }
    }
    else
    {
        if (cmd.h3mod != 0.0f)
        {
            glTexCoord2f(0.0f, 0.0f);
            glVertex2f(s_sittingCharacterOffset, 0);
            glTexCoord2f(1.0f, 0.0f);
            glVertex2f(widthOffset, 0);
            glTexCoord2f(0.0f, cmd.h3mod);
            glVertex2f(s_sittingCharacterOffset, h03);
            glTexCoord2f(1.0f, cmd.h3mod);
            glVertex2f(widthOffset, h03);
        }

        if (cmd.h6mod != 0.0f)
        {
            if (cmd.h3mod == 0.0f)
            {
                glTexCoord2f(0.0f, 0.0f);
                glVertex2f(s_sittingCharacterOffset, 0);
                glTexCoord2f(1.0f, 0.0f);
                glVertex2f(width + s_sittingCharacterOffset, 0);
            }

            glTexCoord2f(0.0f, cmd.h6mod);
            glVertex2f(0, h06);
            glTexCoord2f(1.0f, cmd.h6mod);
            glVertex2f(width, h06);
        }

        if (cmd.h9mod != 0.0f)
        {
            if (cmd.h6mod == 0.0f)
            {
                glTexCoord2f(0.0f, 0.0f);
                glVertex2f(0, 0);
                glTexCoord2f(1.0f, 0.0f);
                glVertex2f(width, 0);
            }

            glTexCoord2f(0.0f, 1.0f);
            glVertex2f(0, h09);
            glTexCoord2f(1.0f, 1.0f);
            glVertex2f(width, h09);
        }
    }

    glEnd();

    glTranslatef(-x, -y, 0.0f);

    return true;
}

bool RenderDraw_DrawLandTile(const DrawLandTileCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);

    RenderState_SetTexture(state, TextureType::TextureType_Texture2D, cmd.texture);
    float translateX = cmd.x - 22.0f;
    float translateY = cmd.y - 22.0f;

    const auto &rc = cmd.rect;

    glTranslatef(translateX, translateY, 0.0f);

    glBegin(GL_TRIANGLE_STRIP);
    glNormal3f(cmd.normals[0][0], cmd.normals[0][1], cmd.normals[0][2]);
    glTexCoord2i(0, 0);
    glVertex2i(22, -rc.x); //^

    glNormal3f(cmd.normals[3][0], cmd.normals[3][1], cmd.normals[3][2]);
    glTexCoord2i(0, 1);
    glVertex2i(0, 22 - rc.y); //<

    glNormal3f(cmd.normals[1][0], cmd.normals[1][1], cmd.normals[1][2]);
    glTexCoord2i(1, 0);
    glVertex2i(44, 22 - rc.height); //>

    glNormal3f(cmd.normals[2][0], cmd.normals[2][1], cmd.normals[2][2]);
    glTexCoord2i(1, 1);
    glVertex2i(22, 44 - rc.width); //v
    glEnd();

    glTranslatef(-translateX, -translateY, 0.0f);
    return true;
}

bool RenderDraw_DrawShadow(const DrawShadowCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);

    RenderState_SetShaderUniform(
        state, cmd.uniformId, &cmd.uniformValue, ShaderUniformType::ShaderUniformType_Int1);
    RenderState_SetBlend(
        state,
        true,
        BlendFactor::BlendFactor_DstColor,
        BlendFactor::BlendFactor_Zero,
        BlendEquation::BlendEquation_Add);

    RenderState_SetTexture(state, TextureType::TextureType_Texture2D, cmd.texture);

    auto width = (float)cmd.width;
    auto height = cmd.height / 2.0f;

    auto x = GLfloat(cmd.x);
    auto translateY = GLfloat(cmd.y + height * 0.75);

    glTranslatef(x, translateY, 0.0f);

    glBegin(GL_TRIANGLE_STRIP);

    float ratio = height / width;

    if (cmd.mirror)
    {
        glTexCoord2f(0, 1);
        glVertex2f(width, height);
        glTexCoord2f(1, 1);
        glVertex2f(0, height);
        glTexCoord2f(0, 0);
        glVertex2f(width * (ratio + 1.0f), 0);
        glTexCoord2f(1, 0);
        glVertex2f(width * ratio, 0);
    }
    else
    {
        glTexCoord2f(0, 1);
        glVertex2f(0, height);
        glTexCoord2f(1, 1);
        glVertex2f(width, height);
        glTexCoord2f(0, 0);
        glVertex2f(width * ratio, 0);
        glTexCoord2f(1, 0);
        glVertex2f(width * (ratio + 1.0f), 0);
    }

    glEnd();

    glTranslatef(-x, -translateY, 0.0f);

    if (cmd.restoreBlendFunc)
    {
        RenderState_SetBlend(
            state,
            true,
            BlendFactor::BlendFactor_SrcAlpha,
            BlendFactor::BlendFactor_OneMinusSrcAlpha,
            BlendEquation::BlendEquation_Add);
    }
    else
    {
        RenderState_SetBlend(
            state,
            false,
            BlendFactor::BlendFactor_Invalid,
            BlendFactor::BlendFactor_Invalid,
            BlendEquation::BlendEquation_Invalid);
    }

    return true;
}

bool RenderDraw_DrawCircle(const DrawCircleCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);

    glDisable(GL_TEXTURE_2D);

    glTranslatef((GLfloat)cmd.x, (GLfloat)cmd.y, 0.0f);

    glBegin(GL_TRIANGLE_FAN);

    glVertex2i(0, 0);

    if (cmd.gradientMode != 0)
    {
        RenderState_SetColor(state, { 0.f, 0.f, 0.f, 0.f });
    }

    float pi = (float)XUO_M_PI * 2.0f;

    auto radius = cmd.radius;
    for (int i = 0; i <= 360; i++)
    {
        float a = (i / 180.0f) * pi;
        glVertex2f(cos(a) * radius, sin(a) * radius);
    }

    glEnd();

    glTranslatef((GLfloat)-cmd.x, (GLfloat)-cmd.y, 0.0f);

    glEnable(GL_TEXTURE_2D);

    return true;
}

bool RenderDraw_DrawUntexturedQuad(const DrawUntexturedQuadCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);

    glDisable(GL_TEXTURE_2D);

    glTranslatef((GLfloat)cmd.x, (GLfloat)cmd.y, 0.0f);

    auto colored = memcmp(g_ColorInvalid.rgba, cmd.color.rgba, sizeof(g_ColorInvalid.rgba)) != 0;
    auto blend = colored && cmd.color[3] < 1.f;
    if (colored)
    {
        RenderState_SetColor(state, cmd.color);

        if (blend)
        {
            RenderState_SetBlend(
                state,
                true,
                BlendFactor::BlendFactor_SrcAlpha,
                BlendFactor::BlendFactor_OneMinusSrcAlpha,
                BlendEquation::BlendEquation_Add);
        }
    }

    glBegin(GL_TRIANGLE_STRIP);
    glVertex2i(0, cmd.height);
    glVertex2i(cmd.width, cmd.height);
    glVertex2i(0, 0);
    glVertex2i(cmd.width, 0);
    glEnd();

    if (colored)
    {
        if (blend)
        {
            RenderState_SetBlend(
                state,
                false,
                BlendFactor::BlendFactor_Invalid,
                BlendFactor::BlendFactor_Invalid,
                BlendEquation::BlendEquation_Invalid);
        }

        RenderState_SetColor(state, g_ColorWhite);
    }

    glTranslatef((GLfloat)-cmd.x, (GLfloat)-cmd.y, 0.0f);

    glEnable(GL_TEXTURE_2D);

    return true;
}

bool RenderDraw_DrawLine(const DrawLineCmd &cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);

    glDisable(GL_TEXTURE_2D);

    auto colored = memcmp(g_ColorInvalid.rgba, cmd.color.rgba, sizeof(g_ColorInvalid.rgba)) != 0;
    auto blend = colored && cmd.color[3] < 1.f;
    if (colored)
    {
        RenderState_SetColor(state, cmd.color);

        if (blend)
        {
            RenderState_SetBlend(
                state,
                true,
                BlendFactor::BlendFactor_SrcAlpha,
                BlendFactor::BlendFactor_OneMinusSrcAlpha,
                BlendEquation::BlendEquation_Add);
        }
    }

    glBegin(GL_LINES);
    glVertex2i(cmd.x0, cmd.y0);
    glVertex2i(cmd.x1, cmd.y1);
    glEnd();

    if (colored)
    {
        if (blend)
        {
            RenderState_SetBlend(
                state,
                false,
                BlendFactor::BlendFactor_Invalid,
                BlendFactor::BlendFactor_Invalid,
                BlendEquation::BlendEquation_Invalid);
        }

        RenderState_SetColor(state, g_ColorWhite);
    }

    glEnable(GL_TEXTURE_2D);

    return true;
}

bool RenderDraw_AlphaTest(const AlphaTestCmd &cmd, RenderState *state)
{
    return RenderState_SetAlphaTest(state, true, cmd.func, cmd.ref);
}

bool RenderDraw_DisableAlphaTest(const DisableAlphaTestCmd &, RenderState *state)
{
    return RenderState_SetAlphaTest(state, false, AlphaTestFunc::AlphaTestFunc_Invalid, 0.f);
}

bool RenderDraw_BlendState(const BlendStateCmd &cmd, RenderState *state)
{
    return RenderState_SetBlend(state, true, cmd.src, cmd.dst, cmd.equation);
}

bool RenderDraw_DisableBlendState(const DisableBlendStateCmd &, RenderState *state)
{
    return RenderState_SetBlend(
        state,
        false,
        BlendFactor::BlendFactor_Invalid,
        BlendFactor::BlendFactor_Invalid,
        BlendEquation::BlendEquation_Invalid);
}

bool RenderDraw_FlushState(const FlushStateCmd &cmd, RenderState *state)
{
    return RenderState_FlushState(state);
}

bool RenderDraw_SetViewParams(const SetViewParamsCmd &cmd, RenderState *state)
{
    return RenderState_SetViewParams(
        state,
        cmd.scene_x,
        cmd.scene_y,
        cmd.scene_width,
        cmd.scene_height,
        cmd.window_width,
        cmd.window_height,
        cmd.camera_nearZ,
        cmd.camera_farZ,
        cmd.scene_scale,
        cmd.proj_flipped_y);
}

bool RenderDraw_SetModelViewTranslation(const SetModelViewTranslationCmd &cmd, RenderState *state)
{
    return RenderState_SetModelViewTranslation(state, cmd.pos);
}

bool RenderDraw_SetScissor(const SetScissorCmd &cmd, RenderState *state)
{
    return RenderState_SetScissor(state, true, cmd.x, cmd.y, cmd.width, cmd.height);
}

bool RenderDraw_DisableScissor(const DisableScissorCmd &cmd, RenderState *state)
{
    return RenderState_SetScissor(state, false, 0, 0, 0, 0);
}

bool RenderDraw_StencilState(const StencilStateCmd &cmd, RenderState *state)
{
    return RenderState_SetStencil(
        state, true, cmd.func, cmd.ref, cmd.mask, cmd.stencilFail, cmd.depthFail, cmd.bothFail);
}

bool RenderDraw_DisableStencilState(const DisableStencilStateCmd &, RenderState *state)
{
    return RenderState_SetStencilEnabled(state, false);
}

bool RenderDraw_EnableStencilState(const EnableStencilStateCmd &, RenderState *state)
{
    return RenderState_SetStencilEnabled(state, true);
}

bool RenderDraw_DepthState(const DepthStateCmd &cmd, RenderState *state)
{
    return RenderState_SetDepth(state, true, cmd.func);
}

bool RenderDraw_DisableDepthState(const DisableDepthStateCmd &, RenderState *state)
{
    return RenderState_SetDepthEnabled(state, false);
}

bool RenderDraw_EnableDepthState(const EnableDepthStateCmd &, RenderState *state)
{
    return RenderState_SetDepthEnabled(state, true);
}

bool RenderDraw_SetColorMask(const SetColorMaskCmd &cmd, RenderState *state)
{
    return RenderState_SetColorMask(state, cmd.mask);
}

bool RenderDraw_SetColor(const SetColorCmd &cmd, RenderState *state)
{
    return RenderState_SetColor(state, cmd.color);
}

bool RenderDraw_SetClearColor(const SetClearColorCmd &cmd, RenderState *state)
{
    return RenderState_SetClearColor(state, cmd.color);
}

bool RenderDraw_ClearRT(const ClearRTCmd &cmd, RenderState *)
{
    auto mask = (cmd.clearMask & ClearRT::ClearRT_Color) == ClearRT::ClearRT_Color ?
                    GL_COLOR_BUFFER_BIT :
                    0;
    mask |= (cmd.clearMask & ClearRT::ClearRT_Depth) == ClearRT::ClearRT_Depth ?
                GL_DEPTH_BUFFER_BIT :
                0;
    mask |= (cmd.clearMask & ClearRT::ClearRT_Stencil) == ClearRT::ClearRT_Stencil ?
                GL_STENCIL_BUFFER_BIT :
                0;
    glClear(mask);

    return true;
}

bool RenderDraw_ShaderUniform(const ShaderUniformCmd &cmd, RenderState *state)
{
    return RenderState_SetShaderUniform(state, cmd.id, cmd.value.data, cmd.type);
}

bool RenderDraw_ShaderLargeUniform(const ShaderLargeUniformCmd &cmd, RenderState *state)
{
    return RenderState_SetShaderLargeUniform(state, cmd.id, cmd.value, cmd.count, cmd.type);
}

bool RenderDraw_ShaderPipeline(const ShaderPipelineCmd &cmd, RenderState *state)
{
    return RenderState_SetShaderPipeline(state, cmd.pipeline);
}

bool RenderDraw_DisableShaderPipeline(const DisableShaderPipelineCmd &cmd, RenderState *state)
{
    return RenderState_DisableShaderPipeline(state);
}

bool RenderDraw_GetFrameBufferPixels(const GetFrameBufferPixelsCmd &cmd, RenderState *state)
{
    const auto format = GL_UNSIGNED_INT_8_8_8_8_REV;
    const auto bpp = 4;
    auto neededSize = (cmd.width * cmd.height) * bpp;
    assert(cmd.dataSize >= neededSize);
    if (cmd.dataSize < neededSize)
    {
        return false;
    }

    // game viewport isn't scaled, if the OS window is smaller than GameWindowPosY + GameWindowHeight, bottom will
    // be negative by this difference
    int needed_height = cmd.y + cmd.height;
    int bottom = cmd.window_height - needed_height;

    glReadPixels(cmd.x, bottom, cmd.width, cmd.height, GL_RGBA, format, cmd.data);

    return true;
}

bool RenderDraw_Execute(RenderCmdList *cmdList)
{
    if (cmdList->immediateMode)
    {
        return false;
    }

    char *cmd = cmdList->data;
    uint32_t remainingCmdSize = cmdList->size - cmdList->remainingSize;
    char *listEnd = cmd + remainingCmdSize;

    while (cmd < listEnd)
    {
        // RenderCommandHeader &cmdHeader = *(RenderCommandHeader *)cmd;
        // switch (cmdHeader.type)
        RenderCommandType type = *(RenderCommandType *)cmd;
        cmd += sizeof(type);
        switch (type)
        {
            MATCH_CASE_DRAW(DrawQuad, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DrawRotatedQuad, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DrawCharacterSitting, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DrawLandTile, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DrawShadow, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DrawCircle, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DrawUntexturedQuad, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DrawLine, cmd, &cmdList->state)
            MATCH_CASE_DRAW(ClearRT, cmd, &cmdList->state)

            MATCH_CASE_DRAW(FlushState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(SetTexture, cmd, &cmdList->state)
            MATCH_CASE_DRAW(SetFrameBuffer, cmd, &cmdList->state)
            MATCH_CASE_DRAW(AlphaTest, cmd, &cmdList->state)
            MATCH_CASE_DRAW(BlendState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DisableBlendState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(StencilState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DisableStencilState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(EnableStencilState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DepthState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DisableDepthState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(EnableDepthState, cmd, &cmdList->state)
            MATCH_CASE_DRAW(SetColorMask, cmd, &cmdList->state)
            MATCH_CASE_DRAW(SetColor, cmd, &cmdList->state);
            MATCH_CASE_DRAW(SetClearColor, cmd, &cmdList->state);
            MATCH_CASE_DRAW(SetViewParams, cmd, &cmdList->state);
            MATCH_CASE_DRAW(SetModelViewTranslation, cmd, &cmdList->state);
            MATCH_CASE_DRAW(SetScissor, cmd, &cmdList->state);
            MATCH_CASE_DRAW(DisableScissor, cmd, &cmdList->state);

            MATCH_CASE_DRAW(ShaderUniform, cmd, &cmdList->state)
            MATCH_CASE_DRAW(ShaderLargeUniform, cmd, &cmdList->state)
            MATCH_CASE_DRAW(ShaderPipeline, cmd, &cmdList->state);
            MATCH_CASE_DRAW(DisableShaderPipeline, cmd, &cmdList->state);

            MATCH_CASE_DRAW(GetFrameBufferPixels, cmd, &cmdList->state);

            default:
                assert(false);
                break;
        }
    }
    return true;
}
#include "Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "Renderer/RenderInternal.h"
#include "Utility/PerfMarker.h"

#define MATCH_CASE_DRAW(type, ret, cmd, state)                                                     \
    case RenderCommandType::Cmd_##type:                                                            \
    {                                                                                              \
        ret &= RenderDraw_##type((type##Cmd *)cmd, state);                                         \
        (char *)cmd += sizeof(type##Cmd);                                                          \
        break;                                                                                     \
    }

bool RenderDraw_SetTexture(SetTextureCmd *cmd, RenderState *state)
{
    return RenderState_SetTexture(state, cmd->type, cmd->texture);
}

bool RenderDraw_DrawQuad(DrawQuadCmd *cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);
    RenderState_SetTexture(state, RenderTextureType::Texture2D, cmd->texture);
    // TODO move this into a new command? into a cached state?
    glColor4f(cmd->rgba[0], cmd->rgba[1], cmd->rgba[2], cmd->rgba[3]);
    glTranslatef((GLfloat)cmd->x, (GLfloat)cmd->y, 0.0f);
    glBegin(GL_TRIANGLE_STRIP);

    if (!cmd->mirrored)
    {
        glTexCoord2f(0.0f, cmd->v);
        glVertex2i(0, cmd->height);
        glTexCoord2f(cmd->u, cmd->v);
        glVertex2i(cmd->width, cmd->height);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i(0, 0);
        glTexCoord2f(cmd->u, 0.0f);
        glVertex2i(cmd->width, 0);
    }
    else
    {
        glTexCoord2f(0.0f, cmd->v);
        glVertex2i(cmd->width, cmd->height);
        glTexCoord2f(cmd->u, cmd->v);
        glVertex2i(0, cmd->height);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i(cmd->width, 0);
        glTexCoord2f(cmd->u, 0.0f);
        glVertex2i(0, 0);
    }

    glEnd();
    glTranslatef(-(GLfloat)cmd->x, -(GLfloat)cmd->y, 0.0f);

    return true;
}

bool RenderDraw_DrawRotatedQuad(DrawRotatedQuadCmd *cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);

    RenderState_SetTexture(state, RenderTextureType::Texture2D, cmd->texture);
    // TODO move this into a new command? into a cached state?
    glColor4f(cmd->rgba[0], cmd->rgba[1], cmd->rgba[2], cmd->rgba[3]);
    glTranslatef((GLfloat)cmd->x, (GLfloat)cmd->y, 0.0f);
    glRotatef(cmd->angle, 0.0f, 0.0f, 1.0f);

    glBegin(GL_TRIANGLE_STRIP);

    if (!cmd->mirrored)
    {
        glTexCoord2f(0.0f, cmd->v);
        glVertex2i(0, cmd->height);
        glTexCoord2f(cmd->u, cmd->v);
        glVertex2i(cmd->width, cmd->height);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i(0, 0);
        glTexCoord2f(cmd->u, 0.0f);
        glVertex2i(cmd->width, 0);
    }
    else
    {
        glTexCoord2f(0.0f, cmd->v);
        glVertex2i(cmd->width, cmd->height);
        glTexCoord2f(cmd->u, cmd->v);
        glVertex2i(0, cmd->height);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2i(cmd->width, 0);
        glTexCoord2f(cmd->u, 0.0f);
        glVertex2i(0, 0);
    }

    glEnd();

    glTranslatef(-(GLfloat)cmd->x, -(GLfloat)cmd->y, 0.0f);
    glRotatef(cmd->angle, 0.0f, 0.0f, -1.0f);

    return true;
}

bool RenderDraw_DrawCharacterSitting(DrawCharacterSittingCmd *cmd, RenderState *state)
{
    static const auto s_sittingCharacterOffset = 8.f;

    ScopedPerfMarker(__FUNCTION__);

    RenderState_SetTexture(state, RenderTextureType::Texture2D, cmd->texture);

    auto x = (GLfloat)cmd->x;
    auto y = (GLfloat)cmd->y;
    float width = (float)cmd->width;
    float height = (float)cmd->height;
    float h03 = height * cmd->h3mod;
    float h06 = height * cmd->h6mod;
    float h09 = height * cmd->h9mod;
    float widthOffset = (float)(width + s_sittingCharacterOffset);

    glTranslatef(x, y, 0.0f);

    glBegin(GL_TRIANGLE_STRIP);

    if (cmd->mirror)
    {
        // TODO this won't end well... add a isAlmostZeroF(float val)
        if (cmd->h3mod != 0.0f)
        {
            glTexCoord2f(0.0f, 0.0f);
            glVertex2f(width, 0);
            glTexCoord2f(1.0f, 0.0f);
            glVertex2f(0, 0);
            glTexCoord2f(0.0f, cmd->h3mod);
            glVertex2f(width, h03);
            glTexCoord2f(1.0f, cmd->h3mod);
            glVertex2f(0, h03);
        }

        if (cmd->h6mod != 0.0f)
        {
            if (cmd->h3mod == 0.0f)
            {
                glTexCoord2f(0.0f, 0.0f);
                glVertex2f(width, 0);
                glTexCoord2f(1.0f, 0.0f);
                glVertex2f(0, 0);
            }

            glTexCoord2f(0.0f, cmd->h6mod);
            glVertex2f(widthOffset, h06);
            glTexCoord2f(1.0f, cmd->h6mod);
            glVertex2f(s_sittingCharacterOffset, h06);
        }

        if (cmd->h9mod != 0.0f)
        {
            if (cmd->h6mod == 0.0f)
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
        if (cmd->h3mod != 0.0f)
        {
            glTexCoord2f(0.0f, 0.0f);
            glVertex2f(s_sittingCharacterOffset, 0);
            glTexCoord2f(1.0f, 0.0f);
            glVertex2f(widthOffset, 0);
            glTexCoord2f(0.0f, cmd->h3mod);
            glVertex2f(s_sittingCharacterOffset, h03);
            glTexCoord2f(1.0f, cmd->h3mod);
            glVertex2f(widthOffset, h03);
        }

        if (cmd->h6mod != 0.0f)
        {
            if (cmd->h3mod == 0.0f)
            {
                glTexCoord2f(0.0f, 0.0f);
                glVertex2f(s_sittingCharacterOffset, 0);
                glTexCoord2f(1.0f, 0.0f);
                glVertex2f(width + s_sittingCharacterOffset, 0);
            }

            glTexCoord2f(0.0f, cmd->h6mod);
            glVertex2f(0, h06);
            glTexCoord2f(1.0f, cmd->h6mod);
            glVertex2f(width, h06);
        }

        if (cmd->h9mod != 0.0f)
        {
            if (cmd->h6mod == 0.0f)
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

bool RenderDraw_DrawLandTile(DrawLandTileCmd *cmd, RenderState *state)
{
    ScopedPerfMarker(__FUNCTION__);

    RenderState_SetTexture(state, RenderTextureType::Texture2D, cmd->texture);
    float translateX = cmd->x - 22.0f;
    float translateY = cmd->y - 22.0f;

    const auto &rc = cmd->rect;

    glTranslatef(translateX, translateY, 0.0f);

    glBegin(GL_TRIANGLE_STRIP);
    glNormal3f(cmd->normals[0][0], cmd->normals[0][1], cmd->normals[0][2]);
    glTexCoord2i(0, 0);
    glVertex2i(22, -rc.x); //^

    glNormal3f(cmd->normals[3][0], cmd->normals[3][1], cmd->normals[3][2]);
    glTexCoord2i(0, 1);
    glVertex2i(0, 22 - rc.y); //<

    glNormal3f(cmd->normals[1][0], cmd->normals[1][1], cmd->normals[1][2]);
    glTexCoord2i(1, 0);
    glVertex2i(44, 22 - rc.height); //>

    glNormal3f(cmd->normals[2][0], cmd->normals[2][1], cmd->normals[2][2]);
    glTexCoord2i(1, 1);
    glVertex2i(22, 44 - rc.width); //v
    glEnd();

    glTranslatef(-translateX, -translateY, 0.0f);
    return true;
}

bool RenderDraw_BlendState(BlendStateCmd *cmd, RenderState *state)
{
    return RenderState_SetBlend(state, true, cmd->func);
}

bool RenderDraw_DisableBlendState(DisableBlendStateCmd *, RenderState *state)
{
    return RenderState_SetBlend(state, false, BlendFunc::BlendFunc_Invalid);
}

bool RenderDraw_FlushState(FlushStateCmd *cmd, RenderState *state)
{
    return RenderState_FlushState(state);
}

bool RenderDraw_StencilState(StencilStateCmd *cmd, RenderState *state)
{
    return RenderState_SetStencil(
        state,
        true,
        cmd->func,
        cmd->ref,
        cmd->mask,
        cmd->stencilFail,
        cmd->depthFail,
        cmd->bothFail);
}

bool RenderDraw_DisableStencilState(DisableStencilStateCmd *, RenderState *state)
{
    return RenderState_SetStencil(
        state,
        false,
        StencilFunc::StencilFunc_Invalid,
        0,
        0,
        StencilOp::StencilOp_Invalid,
        StencilOp::StencilOp_Invalid,
        StencilOp::StencilOp_Invalid);
}

bool RenderDraw_SetColorMask(SetColorMaskCmd *cmd, RenderState *state)
{
    return RenderState_SetColorMask(state, cmd->mask);
}

bool RenderDraw_ClearRT(ClearRTCmd *cmd, RenderState *)
{
    auto mask = cmd->clearMask & ClearRT::Color ? GL_COLOR_BUFFER_BIT : 0;
    mask = mask | cmd->clearMask & ClearRT::Depth ? GL_DEPTH_BUFFER_BIT : 0;
    mask = mask | cmd->clearMask & ClearRT::Stencil ? GL_STENCIL_BUFFER_BIT : 0;
    glClear(mask);

    return true;
}

bool RenderDraw_ShaderUniform(ShaderUniformCmd *cmd, RenderState *state)
{
    return RenderState_SetShaderUniform(state, cmd->id, cmd->value, cmd->type);
}

bool RenderDraw_ShaderLargeUniform(ShaderLargeUniformCmd *cmd, RenderState *state)
{
    return RenderState_SetShaderLargeUniform(state, cmd->id, cmd->value, cmd->count, cmd->type);
}

bool RenderDraw_ShaderPipeline(ShaderPipelineCmd *cmd, RenderState *state)
{
    return RenderState_SetShaderPipeline(state, cmd->pipeline);
}

bool RenderDraw_DisableShaderPipeline(DisableShaderPipelineCmd *cmd, RenderState *state)
{
    return RenderState_DisableShaderPipeline(state);
}

bool RenderDraw_Execute(RenderCmdList *cmdList)
{
    assert(!cmdList->immediateMode);
    if (cmdList->immediateMode)
    {
        return false;
    }

    char *cmd = cmdList->data;
    uint32_t remainingCmdSize = cmdList->size - cmdList->remainingSize;
    char *listEnd = cmd + remainingCmdSize;

    bool ret = true;
    while (cmd < listEnd && ret)
    {
        RenderCommandHeader &cmdHeader = *(RenderCommandHeader *)cmd;
        switch (cmdHeader.type)
        {
            MATCH_CASE_DRAW(DrawQuad, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DrawRotatedQuad, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DrawCharacterSitting, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DrawLandTile, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(ClearRT, ret, cmd, &cmdList->state)

            MATCH_CASE_DRAW(FlushState, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(SetTexture, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(BlendState, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DisableBlendState, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(StencilState, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DisableStencilState, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(SetColorMask, ret, cmd, &cmdList->state)

            MATCH_CASE_DRAW(ShaderUniform, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(ShaderLargeUniform, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(ShaderPipeline, ret, cmd, &cmdList->state);
            MATCH_CASE_DRAW(DisableShaderPipeline, ret, cmd, &cmdList->state);

            default:
                assert(false);
                break;
        }
    }
    return true;
}
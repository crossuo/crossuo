#include "Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "Renderer/RenderInternal.h"

#define MATCH_CASE_DRAW(type, ret, cmd, state)                                                     \
    case RenderCommandType::Cmd_##type:                                                            \
    {                                                                                              \
        ret &= RenderDraw_##type((type##Cmd *)cmd, state);                                         \
        (char *)cmd += sizeof(type##Cmd);                                                          \
        break;                                                                                     \
    }

#define MATCH_CASE_DRAW_DEBUG(type, cmd, state)                                                    \
    case RenderCommandType::Cmd_##type:                                                            \
    {                                                                                              \
        RenderDraw_##type##Debug((type##Cmd *)cmd, state);                                         \
        (char *)cmd += sizeof(type##Cmd);                                                          \
        break;                                                                                     \
    }

#define OGL_USERPERFMARKERS_ID (0xbabebabe)
bool RenderDraw_Texture(TextureCmd *cmd, RenderState *state)
{
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, OGL_USERPERFMARKERS_ID, -1, __FUNCTION__);
    RenderState_SetTexture(state, RenderTextureType::Texture2D, cmd->texture);
    // TODO move this into a new command? into a cached state?
    glColor4f(cmd->rgba[0], cmd->rgba[1], cmd->rgba[2], cmd->rgba[3]);
    glTranslatef((GLfloat)cmd->x, (GLfloat)cmd->y, 0.0f);
    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0.0f, cmd->v);
    glVertex2i(0, cmd->height);
    glTexCoord2f(cmd->u, cmd->v);
    glVertex2i(cmd->width, cmd->height);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2i(0, 0);
    glTexCoord2f(cmd->u, 0.0f);
    glVertex2i(cmd->width, 0);
    glEnd();
    glTranslatef(-(GLfloat)cmd->x, -(GLfloat)cmd->y, 0.0f);

    glPopDebugGroup();
    return true;
}

bool RenderDraw_RotatedTexture(RotatedTextureCmd *cmd, RenderState *state)
{
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, OGL_USERPERFMARKERS_ID, -1, __FUNCTION__);
    RenderState_SetTexture(state, RenderTextureType::Texture2D, cmd->texture);
    // TODO move this into a new command? into a cached state?
    glColor4f(cmd->rgba[0], cmd->rgba[1], cmd->rgba[2], cmd->rgba[3]);
    glTranslatef((GLfloat)cmd->x, (GLfloat)cmd->y, 0.0f);
    glRotatef(cmd->angle, 0.0f, 0.0f, 1.0f);

    glBegin(GL_TRIANGLE_STRIP);
    glTexCoord2f(0.0f, cmd->v);
    glVertex2i(0, cmd->height);
    glTexCoord2f(cmd->u, cmd->v);
    glVertex2i(cmd->width, cmd->height);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2i(0, 0);
    glTexCoord2f(cmd->u, 0.0f);
    glVertex2i(cmd->width, 0);
    glEnd();

    glTranslatef(-(GLfloat)cmd->x, -(GLfloat)cmd->y, 0.0f);
    glRotatef(cmd->angle, 0.0f, 0.0f, -1.0f);

    glPopDebugGroup();
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

bool RenderDraw_StencilState(StencilStateCmd *, RenderState *state)
{
    return RenderState_SetStencil(state, true);
}

bool RenderDraw_DisableStencilState(DisableStencilStateCmd *, RenderState *state)
{
    return RenderState_SetStencil(state, false);
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
            MATCH_CASE_DRAW(Texture, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(RotatedTexture, ret, cmd, &cmdList->state)

            MATCH_CASE_DRAW(FlushState, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(BlendState, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DisableBlendState, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(StencilState, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DisableStencilState, ret, cmd, &cmdList->state)

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
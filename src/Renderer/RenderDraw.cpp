#include "Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "Renderer/RenderInternal.h"

#define MATCH_CASE_DRAW(type, ret, cmd, state)                                                     \
    case RenderCommandType::##type:                                                                \
    {                                                                                              \
        ret &= RenderDraw_##type((type##Cmd *)cmd, state);                                         \
        (char *)cmd += sizeof(type##Cmd);                                                          \
        break;                                                                                     \
    }

bool RenderDraw_Texture(TextureCmd *cmd, RenderState *state)
{
    Info(Renderer, "drawing texture cmd: %p\n", cmd);

    RenderState_SetTexture(state, RenderTextureType::Texture2D, cmd->texture);

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

    return true;
}

bool RenderDraw_BlendState(BlendStateCmd *cmd, RenderState *state)
{
    return RenderState_SetBlend(state, cmd->enabled, cmd->func);
}

bool RenderDraw_FlushState(FlushStateCmd *cmd, RenderState *state)
{
    return RenderState_FlushState(state);
}

bool RenderDraw_StencilState(StencilStateCmd *cmd, RenderState *state)
{
    return RenderState_SetStencil(state, cmd->enabled);
}

bool RenderDraw_Execute(RenderCmdList *cmdList, RenderState *state)
{
    char *cmd = cmdList->data;
    uint32_t remainingCmdSize = cmdList->size - cmdList->remainingSize;
    char *listEnd = cmd + remainingCmdSize;

    bool ret = true;
    while (cmd < listEnd && ret)
    {
        RenderCommandHeader &cmdHeader = *(RenderCommandHeader *)cmd;
        switch (cmdHeader.type)
        {
            MATCH_CASE_DRAW(Texture, ret, cmd, state)

            MATCH_CASE_DRAW(FlushState, ret, cmd, state)
            MATCH_CASE_DRAW(BlendState, ret, cmd, state)
            MATCH_CASE_DRAW(StencilState, ret, cmd, state)

            default:
                assert(false);
                break;
        }
    }
    return true;
}
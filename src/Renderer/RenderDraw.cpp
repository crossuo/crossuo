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

#define MATCH_CASE_DRAW_DEBUG(type, cmd, state)                                                    \
    case RenderCommandType::##type:                                                                \
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

bool RenderDraw_BlendState(BlendStateCmd *cmd, RenderState *state)
{
    return RenderState_SetBlend(state, true, cmd->func);
}

bool RenderDraw_DisableBlendState(DisableBlendStateCmd *, RenderState *state)
{
    return RenderState_SetBlend(state, false, BlendFunc::Invalid);
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

void RenderDraw_DumpCmdList(RenderCmdList *cmdList)
{
    Info(
        Renderer,
        "Dumping cmd list %p, data %p, capacity %dkB, free size %dkB, immediate? %s",
        cmdList,
        cmdList->data,
        cmdList->size,
        cmdList->remainingSize,
        cmdList->immediateMode);

    static auto s_blendFuncToString = [](BlendFunc func) {
        static const char *s_funcToText[] = { "SrcAlpha_OneMinusSrcAlpha" };
        static_assert(countof(s_funcToText) == BlendFunc::Count);

        if (func == BlendFunc::Invalid)
        {
            return "Invalid";
        }

        assert(func < BlendFunc::Count);
        return s_funcToText[func];
    };

    auto RenderDraw_TextureDebug = [](TextureCmd *cmd, RenderState *state) {
        Info(
            Renderer,
            "TextureCmd: texture: %d - x: %d - y: %d - width: %d - height: %d - u: %f - v: %f - rgba: [%f, %f, %f, %f]\n",
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
            cmd->rgba[3]);
    };

    auto RenderDraw_FlushStateDebug = [](FlushStateCmd *, RenderState *state) {
        Info(Renderer, "FlushStateCmd\n");
    };

    auto RenderDraw_BlendStateDebug = [](BlendStateCmd *cmd, RenderState *state) {
        Info(Renderer, "BlendStateCmd: func: %s\n", s_blendFuncToString(cmd->func));
    };

    auto RenderDraw_DisableBlendStateDebug = [](DisableBlendStateCmd *cmd, RenderState *state) {
        Info(Renderer, "DisableBlendStateCmd\n");
    };

    auto RenderDraw_StencilStateDebug = [](StencilStateCmd *cmd, RenderState *state) {
        Info(Renderer, "StencilStateCmd\n");
    };

    auto RenderDraw_DisableStencilStateDebug = [](DisableStencilStateCmd *cmd, RenderState *state) {
        Info(Renderer, "DisableStencilStateCmd\n");
    };

    char *cmd = cmdList->data;
    uint32_t remainingCmdSize = cmdList->size - cmdList->remainingSize;
    char *listEnd = cmd + remainingCmdSize;

    while (cmd < listEnd)
    {
        RenderCommandHeader &cmdHeader = *(RenderCommandHeader *)cmd;
        switch (cmdHeader.type)
        {
            MATCH_CASE_DRAW_DEBUG(Texture, cmd, &cmdList->state)

            MATCH_CASE_DRAW_DEBUG(FlushState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(BlendState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DisableBlendState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(StencilState, cmd, &cmdList->state)
            MATCH_CASE_DRAW_DEBUG(DisableStencilState, cmd, &cmdList->state)

            default:
                assert(false);
                break;
        }
    }
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

            MATCH_CASE_DRAW(FlushState, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(BlendState, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DisableBlendState, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(StencilState, ret, cmd, &cmdList->state)
            MATCH_CASE_DRAW(DisableStencilState, ret, cmd, &cmdList->state)

            default:
                assert(false);
                break;
        }
    }
    return true;
}
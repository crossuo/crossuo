#include "Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "Renderer/RenderInternal.h"

float4 g_ColorWhite = { { 1.f, 1.f, 1.f, 1.f } };

bool Render_AppendCmd(RenderCmdList *cmdList, void *cmd, uint32_t cmdSize)
{
    assert(cmdList);
    assert(cmd);
    assert(cmdSize);

    if (cmdList->remainingSize >= cmdSize)
    {
        memcpy(cmdList->data + cmdList->size - cmdList->remainingSize, cmd, cmdSize);
        cmdList->remainingSize -= cmdSize;
        return true;
    }

    Error(Renderer, __FUNCTION__ " render cmd list capacity reached. skipping render cmd %p", cmd);
    return false;
}

RenderCmdList
Render_CmdList(void *buffer, uint32_t bufferSize, RenderState state, bool immediateMode)
{
    assert(buffer);
    assert(bufferSize);

    return RenderCmdList(buffer, bufferSize, state, immediateMode);
}

RenderState Render_DefaultState()
{
    return RenderState();
}

void Render_ResetCmdList(RenderCmdList *cmdList, RenderState state)
{
    assert(cmdList);

    cmdList->remainingSize = cmdList->size;
    cmdList->state = state;
}
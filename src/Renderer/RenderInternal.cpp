#include "Renderer/RenderAPI.h"
#define RENDERER_INTERNAL
#include "Renderer/RenderInternal.h"

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

    return false;
}

RenderCmdList Render_CmdList(void *buffer, uint32_t bufferSize)
{
    assert(buffer);
    assert(bufferSize);

    return RenderCmdList(buffer, bufferSize);
}

RenderState Render_DefaultState()
{
    return RenderState();
}

void Render_ResetCmdList(RenderCmdList *cmdList)
{
    assert(cmdList);

    cmdList->remainingSize = cmdList->size;
}
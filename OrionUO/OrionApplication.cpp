// MIT License
// Copyright (C) August 2016 Hotride

#include <SDL_timer.h>

COrionApplication g_App;

void COrionApplication::OnMainLoop()
{
    //DEBUG_TRACE_FUNCTION;
    g_Ticks = SDL_GetTicks();

#if USE_TIMERTHREAD
    if (NextRenderTime <= g_Ticks)
    {
        NextUpdateTime = g_Ticks + 50;
        NextRenderTime = NextUpdateTime; // g_Ticks + g_OrionWindow.RenderTimerDelay;
        g_ConnectionManager.Recv();
        g_PacketManager.ProcessPluginPackets();
        g_PacketManager.SendMegaClilocRequests();
    }
    else if (NextUpdateTime <= g_Ticks)
    {
        NextUpdateTime = g_Ticks + 50;
        g_ConnectionManager.Recv();
        g_PacketManager.ProcessPluginPackets();
        g_PacketManager.SendMegaClilocRequests();
    }
#else
    g_ConnectionManager.Recv();
    g_PacketManager.ProcessPluginPackets();
    g_PacketManager.SendMegaClilocRequests();
    g_Orion.Process(true);
#endif // USE_TIMERTHREAD
}

// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** OrionApplication.cpp
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#include "stdafx.h"
#include <SDL_timer.h>

COrionApplication g_App;

void COrionApplication::OnMainLoop()
{
    //DEBUG_TRACE_FUNCTION;
    g_Ticks = SDL_GetTicks();

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
}

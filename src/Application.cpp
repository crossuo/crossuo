// MIT License
// Copyright (C) August 2016 Hotride

#include <SDL_timer.h>
#include "Application.h"
#include "CrossUO.h"
#include "Managers/ConnectionManager.h"
#include "Managers/PacketManager.h"

CApplication g_App;

void CApplication::OnMainLoop()
{
    //DEBUG_TRACE_FUNCTION;
    g_Ticks = SDL_GetTicks();
    g_ConnectionManager.Recv();
    g_PacketManager.ProcessPluginPackets();
    g_PacketManager.SendMegaClilocRequests();
    g_Game.Process(true);
}

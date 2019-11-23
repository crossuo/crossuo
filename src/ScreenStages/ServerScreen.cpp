// MIT License
// Copyright (C) August 2016 Hotride

#include "ServerScreen.h"
#include "MainScreen.h"
#include "../CrossUO.h"
#include "../ServerList.h"
#include "../GameWindow.h"
#include "../Managers/ScreenEffectManager.h"

CServerScreen g_ServerScreen;

CServerScreen::CServerScreen()
    : CBaseScreen(m_ServerGump)
{
}

CServerScreen::~CServerScreen()
{
}

void CServerScreen::Init()
{
    DEBUG_TRACE_FUNCTION;
    CBaseScreen::Init();

    g_GameWindow.SetTitle(std::string("Ultima Online - ") + g_MainScreen.m_Account->c_str());
    g_ScreenEffectManager.UseSunrise();
    SmoothScreenAction = 0;

    m_Gump.PrepareTextures();
    m_Gump.WantUpdateContent = true;
}

void CServerScreen::OnKeyDown(const KeyEvent &ev)
{
    DEBUG_TRACE_FUNCTION;

    m_Gump.OnKeyDown(ev);

    const auto key = EvKey(ev);
    if (key == KEY_RETURN || key == KEY_RETURN2)
    {
        SelectionServerTempValue = g_ServerList.LastServerIndex;
        CreateSmoothAction(ID_SMOOTH_SS_SELECT_SERVER);
    }
}

void CServerScreen::ProcessSmoothAction(uint8_t action)
{
    DEBUG_TRACE_FUNCTION;

    if (action == 0xFF)
    {
        action = SmoothScreenAction;
    }

    if (action == ID_SMOOTH_SS_SELECT_SERVER)
    {
        g_Game.ServerSelection(SelectionServerTempValue);
    }
    else if (action == ID_SMOOTH_SS_QUIT)
    {
        g_GameWindow.Destroy();
    }
    else if (action == ID_SMOOTH_SS_GO_SCREEN_MAIN)
    {
        g_Game.Disconnect();
        g_Game.InitScreen(GS_MAIN);
    }
}

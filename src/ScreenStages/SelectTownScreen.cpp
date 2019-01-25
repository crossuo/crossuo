// MIT License
// Copyright (C) August 2016 Hotride

#include "SelectTownScreen.h"
#include "CreateCharacterScreen.h"
#include "../Config.h"
#include "../CrossUO.h"
#include "../CityList.h"
#include "../GameWindow.h"
#include "../Managers/ScreenEffectManager.h"
#include "../Network/Packets.h"

CSelectTownScreen g_SelectTownScreen;

CSelectTownScreen::CSelectTownScreen()
    : CBaseScreen(m_SelectTownGump)
{
}

CSelectTownScreen::~CSelectTownScreen()
{
}

void CSelectTownScreen::Init()
{
    DEBUG_TRACE_FUNCTION;
    if (g_Config.ClientVersion >= CV_70130)
    {
        m_City = g_CityList.GetCity(0);
    }
    else
    {
        m_City = g_CityList.GetCity(3);

        if (m_City == nullptr)
        {
            m_City = g_CityList.GetCity(0);
        }
    }

    g_ScreenEffectManager.UseSunrise();
    SmoothScreenAction = 0;

    m_Gump.PrepareTextures();
    m_Gump.WantUpdateContent = true;
}

void CSelectTownScreen::ProcessSmoothAction(uint8_t action)
{
    DEBUG_TRACE_FUNCTION;
    if (action == 0xFF)
    {
        action = SmoothScreenAction;
    }

    if (action == ID_SMOOTH_STS_QUIT)
    {
        g_GameWindow.Destroy();
    }
    else if (action == ID_SMOOTH_STS_GO_SCREEN_CHARACTER)
    {
        g_Game.InitScreen(GS_CHARACTER);
    }
    else if (action == ID_SMOOTH_STS_GO_SCREEN_GAME_CONNECT)
    {
        CPacketCreateCharacter(g_CreateCharacterScreen.Name).Send();
        g_Game.InitScreen(GS_GAME_CONNECT);
    }
}

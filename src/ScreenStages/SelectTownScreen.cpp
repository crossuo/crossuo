// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

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
= default;

void CSelectTownScreen::Init()
{
    CBaseScreen::Init();

    if (g_Config.ClientVersion >= CV_70130)
    {
        m_City = g_CityList.GetCity(0);
    }
    else if (g_Config.ClientVersion < CV_200)
    {
        //1.25.32 CityChoice: the start locations come from the client's
        //regions.mul table (loaded here so the label/popup are populated
        //before the first content build)
        g_CityList.LoadRegionsOrDefault();
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

    //1.25.32 wizard: CharCreation is page 1 (CityChoice page 2, the
    //scrollchoice popups pages 3-6); set here so rebuilds don't clobber
    //an open popup page
    if (g_Config.ClientVersion < CV_200)
    {
        m_SelectTownGump.Page = 1;
    }

    g_ScreenEffectManager.UseSunrise();
    SmoothScreenAction = 0;

    m_Gump.PrepareTextures();
    m_Gump.WantUpdateContent = true;
}

void CSelectTownScreen::ProcessSmoothAction(uint8_t action)
{
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
    else if (action == ID_SMOOTH_STS_GO_SCREEN_CREATE)
    {
        //1.25.32 wizard: BACK from CharCreation to InitSkill
        g_Game.InitScreen(GS_CREATE);
    }
    else if (action == ID_SMOOTH_STS_GO_SCREEN_GAME_CONNECT)
    {
        CPacketCreateCharacter(g_CreateCharacterScreen.Name).Send();
        g_Game.InitScreen(GS_GAME_CONNECT);
    }
}

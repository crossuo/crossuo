// MIT License
// Copyright (C) August 2016 Hotride

#include "SelectProfessionScreen.h"
#include "ConnectionScreen.h"
#include "../ToolTip.h"
#include "../OrionUO.h"
#include "../OrionWindow.h"
#include "../Managers/ProfessionManager.h"
#include "../Managers/ScreenEffectManager.h"

CSelectProfessionScreen g_SelectProfessionScreen;

CSelectProfessionScreen::CSelectProfessionScreen()
    : CBaseScreen(m_SelectProfessionGump)
{
}

CSelectProfessionScreen::~CSelectProfessionScreen()
{
}

void CSelectProfessionScreen::SetSkillSelection(int val)
{
    m_SkillSelection = val;
    m_Gump.WantUpdateContent = true;
}

void CSelectProfessionScreen::Init()
{
    DEBUG_TRACE_FUNCTION;
    g_ProfessionManager.Selected = (CBaseProfession *)g_ProfessionManager.m_Items;
    m_SkillSelection = 0;

    g_ScreenEffectManager.UseSunrise();
    SmoothScreenAction = 0;

    m_Gump.PrepareTextures();
    m_Gump.WantUpdateContent = true;
}

void CSelectProfessionScreen::ProcessSmoothAction(uint8_t action)
{
    DEBUG_TRACE_FUNCTION;
    if (action == 0xFF)
    {
        action = SmoothScreenAction;
    }

    if (action == ID_SMOOTH_SPS_QUIT)
    {
        g_OrionWindow.Destroy();
    }
    else if (action == ID_SMOOTH_SPS_GO_SCREEN_CHARACTER)
    {
        g_Orion.InitScreen(GS_CHARACTER);
    }
    else if (action == ID_SMOOTH_SPS_GO_SCREEN_GAME_CONNECT)
    {
        g_Orion.InitScreen(GS_GAME_CONNECT);
        g_ConnectionScreen.SetType(CST_SELECT_PROFESSOIN);
    }
    else if (action == ID_SMOOTH_SPS_GO_SCREEN_CREATE)
    {
        g_Orion.InitScreen(GS_CREATE);
    }
}

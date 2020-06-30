// MIT License
// Copyright (C) August 2016 Hotride

#include "CharacterListScreen.h"
#include "ConnectionScreen.h"
#include "MainScreen.h"
#include "../CrossUO.h"
#include "../ServerList.h"
#include "../GameWindow.h"
#include "../CharacterList.h"
#include "../Managers/ScreenEffectManager.h"

CCharacterListScreen g_CharacterListScreen;

CCharacterListScreen::CCharacterListScreen()
    : CBaseScreen(m_CharacterListGump)
{
}

void CCharacterListScreen::Init()
{
    CBaseScreen::Init();
    auto title = astr_t("Ultima Online - ") + g_MainScreen.m_Account->GetTextA();
    CServer *server = g_ServerList.GetSelectedServer();
    if (server != nullptr)
    {
        title += "(" + server->Name + ")";
    }

    g_GameWindow.SetTitle(title);
    g_CharacterList.Selected = 0;
    g_ScreenEffectManager.UseSunrise();
    SmoothScreenAction = 0;

    m_Gump.PrepareTextures();
    m_Gump.WantUpdateContent = true;
}

void CCharacterListScreen::ProcessSmoothAction(uint8_t action)
{
    if (action == 0xff)
    {
        action = SmoothScreenAction;
    }

    if (action == CCharacterListScreen::Quit)
    {
        g_GameWindow.Destroy();
    }
    else if (action == CCharacterListScreen::Connect)
    {
        g_Game.Connect();
    }
    else if (action == CCharacterListScreen::SelectCharacter)
    {
        if (g_CharacterList.GetName(g_CharacterList.Selected).empty())
        {
            g_Game.InitScreen(GS_PROFESSION_SELECT);
        }
        else
        {
            g_Game.CharacterSelection(g_CharacterList.Selected);
        }
    }
    else if (action == CCharacterListScreen::GotoScreenProfession)
    {
        g_Game.InitScreen(GS_PROFESSION_SELECT);
    }
    else if (action == CCharacterListScreen::GotoScreenDelete)
    {
        if (!g_CharacterList.GetSelectedName().empty())
        {
            g_Game.InitScreen(GS_DELETE);
            g_ConnectionScreen.SetType(CST_CHARACTER_LIST);
        }
    }
}

void CCharacterListScreen::OnKeyDown(const KeyEvent &ev)
{
    m_Gump.OnKeyDown(ev);
    const auto key = EvKey(ev);
    if (key == KEY_RETURN || key == KEY_RETURN2)
    {
        CreateSmoothAction(CCharacterListScreen::SelectCharacter);
    }
}

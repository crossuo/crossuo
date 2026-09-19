// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "CharacterListScreen.h"
#include "ConnectionScreen.h"
#include "MainScreen.h"
#include "../CrossUO.h"
#include "../Globals.h"
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

    m_AutoLoginFired = false;
}

void CCharacterListScreen::PrepareContent()
{
    CBaseScreen::PrepareContent();

    // test harness (XUO_TEST_LOGIN): the screen is entered before the character
    // list packet arrives, so wait for the data here instead of in Init()
    if (TestAutoLogin() && !m_AutoLoginFired && SmoothScreenAction == 0 &&
        g_CharacterList.Count > 0)
    {
        // pick the second available character (falls back to the first when
        // only one slot is in use) and enter the world
        int pick = -1;
        int validSeen = 0;
        for (int i = 0; i < g_CharacterList.Count; i++)
        {
            if (!g_CharacterList.GetName(i).empty())
            {
                validSeen++;
                pick = i;
                if (validSeen == 2)
                {
                    break;
                }
            }
        }

        if (pick != -1)
        {
            printf(
                "[auto-login] selecting character '%s' (slot %d)\n",
                g_CharacterList.GetName(pick).c_str(),
                pick);
            fflush(stdout);
            m_AutoLoginFired = true;
            g_CharacterList.Selected = pick;
            CreateSmoothAction(CCharacterListScreen::SelectCharacter);
        }
    }
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

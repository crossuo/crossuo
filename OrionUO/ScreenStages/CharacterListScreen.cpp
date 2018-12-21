// MIT License
// Copyright (C) August 2016 Hotride

#include "CharacterListScreen.h"

CCharacterListScreen g_CharacterListScreen;

CCharacterListScreen::CCharacterListScreen()
    : CBaseScreen(m_CharacterListGump)
{
}

CCharacterListScreen::~CCharacterListScreen()
{
}

void CCharacterListScreen::Init()
{
    DEBUG_TRACE_FUNCTION;

    string title = string("Ultima Online - ") + g_MainScreen.m_Account->c_str();

    CServer *server = g_ServerList.GetSelectedServer();

    if (server != nullptr)
    {
        title += "(" + server->Name + ")";
    }

    g_OrionWindow.SetTitle(title);

    g_CharacterList.Selected = 0;

    g_ScreenEffectManager.UseSunrise();
    SmoothScreenAction = 0;

    m_Gump.PrepareTextures();
    m_Gump.WantUpdateContent = true;
}

void CCharacterListScreen::ProcessSmoothAction(uint8_t action)
{
    DEBUG_TRACE_FUNCTION;
    if (action == 0xFF)
    {
        action = SmoothScreenAction;
    }

    if (action == ID_SMOOTH_CLS_QUIT)
    {
        g_OrionWindow.Destroy();
    }
    else if (action == ID_SMOOTH_CLS_CONNECT)
    {
        g_Orion.Connect();
    }
    else if (action == ID_SMOOTH_CLS_SELECT_CHARACTER)
    {
        if (g_CharacterList.GetName(g_CharacterList.Selected).length() == 0u)
        {
            g_Orion.InitScreen(GS_PROFESSION_SELECT);
        }
        else
        {
            g_Orion.CharacterSelection(g_CharacterList.Selected);
        }
    }
    else if (action == ID_SMOOTH_CLS_GO_SCREEN_PROFESSION_SELECT)
    {
        g_Orion.InitScreen(GS_PROFESSION_SELECT);
    }
    else if (action == ID_SMOOTH_CLS_GO_SCREEN_DELETE)
    {
        if (g_CharacterList.GetSelectedName().length() != 0u)
        {
            g_Orion.InitScreen(GS_DELETE);
            g_ConnectionScreen.SetType(CST_CHARACTER_LIST);
        }
    }
}

void CCharacterListScreen::OnKeyDown(const KeyEvent &ev)
{
    DEBUG_TRACE_FUNCTION;

    m_Gump.OnKeyDown(ev);

    const auto key = EvKey(ev);
    if (key == KEY_RETURN || key == KEY_RETURN2)
    {
        CreateSmoothAction(ID_SMOOTH_CLS_SELECT_CHARACTER);
    }
}

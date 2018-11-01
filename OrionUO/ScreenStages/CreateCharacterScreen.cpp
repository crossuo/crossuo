// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** CreateCharacterScreen.cpp
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#include "stdafx.h"
#include "CreateCharacterScreen.h"

CCreateCharacterScreen g_CreateCharacterScreen;

CCreateCharacterScreen::CCreateCharacterScreen()
    : CBaseScreen(m_CreateCharacterGump)
{
}

CCreateCharacterScreen::~CCreateCharacterScreen()
{
}

void CCreateCharacterScreen::SetStyleSelection(int val)
{
    m_StyleSelection = val;
    m_Gump.WantUpdateContent = true;
}

void CCreateCharacterScreen::SetColorSelection(int val)
{
    m_ColorSelection = val;
    m_Gump.WantUpdateContent = true;
}

void CCreateCharacterScreen::Init()
{
    DEBUG_TRACE_FUNCTION;
    g_CreateCharacterManager.Clear();

    Name = "";
    m_StyleSelection = 0;
    m_ColorSelection = 0;

    g_ScreenEffectManager.UseSunrise();
    SmoothScreenAction = 0;

    m_Gump.PrepareTextures();
    m_Gump.WantUpdateContent = true;
}

void CCreateCharacterScreen::ProcessSmoothAction(uint8_t action)
{
    DEBUG_TRACE_FUNCTION;
    if (action == 0xFF)
        action = SmoothScreenAction;

    if (action == ID_SMOOTH_CCS_QUIT)
        g_OrionWindow.Destroy();
    else if (action == ID_SMOOTH_CCS_GO_SCREEN_CHARACTER)
        g_Orion.InitScreen(GS_CHARACTER);
    else if (action == ID_SMOOTH_CCS_GO_SCREEN_CONNECT)
    {
        g_Orion.InitScreen(GS_GAME_CONNECT);
        g_ConnectionScreen.SetType(CST_GAME);
        g_ConnectionScreen.SetConnectionFailed(true);
        g_ConnectionScreen.SetErrorCode(1);
    }
    else if (action == ID_SMOOTH_CCS_GO_SCREEN_SELECT_TOWN)
        g_Orion.InitScreen(GS_SELECT_TOWN);
}

void CCreateCharacterScreen::OnLeftMouseButtonDown()
{
    DEBUG_TRACE_FUNCTION;
    CBaseScreen::OnLeftMouseButtonDown();

    if (g_SelectedObject.Serial == 0)
    {
        if (m_StyleSelection != 0)
        {
            m_StyleSelection = 0;
            m_Gump.WantUpdateContent = true;
        }
    }
}

#if USE_WISP
void CCreateCharacterScreen::OnCharPress(const WPARAM &wParam, const LPARAM &lParam)
{
    DEBUG_TRACE_FUNCTION;
    if (wParam >= 0x0100 || !g_FontManager.IsPrintASCII((uint8_t)wParam))
        return;
    else if (g_EntryPointer == nullptr)
        return;

    if (g_EntryPointer->Length() < 20) //add char to text field
        g_EntryPointer->Insert((wchar_t)wParam);

    Name = g_EntryPointer->c_str();
    m_Gump.WantRedraw = true;
}
#else
void CCreateCharacterScreen::OnTextInput(const SDL_TextInputEvent &ev)
{
    NOT_IMPLEMENTED; // FIXME
}
#endif

void CCreateCharacterScreen::OnKeyDown(const KeyEvent &ev)
{
    DEBUG_TRACE_FUNCTION;
    
    const auto key = EvKey(ev);
    if (g_EntryPointer != nullptr)
    {
        g_EntryPointer->OnKey(&m_Gump, key);
        Name = g_EntryPointer->c_str();
        m_Gump.WantRedraw = true;
    }
}

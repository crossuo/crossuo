// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** ServerScreen.cpp
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#include "stdafx.h"
#include "ServerScreen.h"

CServerScreen g_ServerScreen;

CServerScreen::CServerScreen()
    : CBaseScreen(m_ServerGump)
{
}

CServerScreen::~CServerScreen()
{
}

/*!
Инициализация
@return 
*/
void CServerScreen::Init()
{
    DEBUG_TRACE_FUNCTION;
    g_OrionWindow.SetTitle(string("Ultima Online - ") + g_MainScreen.m_Account->c_str());

    g_ScreenEffectManager.UseSunrise();
    SmoothScreenAction = 0;

    m_Gump.PrepareTextures();
    m_Gump.WantUpdateContent = true;
}

#if USE_WISP
/*!
Обработка нажатия клавиши
@param [__in] wparam не подписанный параметр
@param [__in] lparam не подписанный параметр
@return 
*/
void CServerScreen::OnKeyDown(const WPARAM &wParam, const LPARAM &lParam)
{
    DEBUG_TRACE_FUNCTION;
    m_Gump.OnKeyDown(wParam, lParam);

    if (wParam == VK_RETURN)
    {
        SelectionServerTempValue = g_ServerList.LastServerIndex;
        CreateSmoothAction(ID_SMOOTH_SS_SELECT_SERVER);
    }
}
#else
void CServerScreen::OnKeyDown(const SDL_KeyboardEvent &ev)
{
    NOT_IMPLEMENTED; // FIXME
}
#endif

/*!
Обработка события после плавного затемнения экрана
@param [__in_opt] action Идентификатор действия
@return 
*/
void CServerScreen::ProcessSmoothAction(uchar action)
{
    DEBUG_TRACE_FUNCTION;
    if (action == 0xFF)
        action = SmoothScreenAction;

    if (action == ID_SMOOTH_SS_SELECT_SERVER)
        g_Orion.ServerSelection(SelectionServerTempValue);
    else if (action == ID_SMOOTH_SS_QUIT)
        g_OrionWindow.Destroy();
    else if (action == ID_SMOOTH_SS_GO_SCREEN_MAIN)
    {
        g_Orion.Disconnect();
        g_Orion.InitScreen(GS_MAIN);
    }
}


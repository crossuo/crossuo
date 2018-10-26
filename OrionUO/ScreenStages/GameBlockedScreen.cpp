// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** GameBlockedScreen.cpp
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#include "stdafx.h"
#include "GameBlockedScreen.h"

CGameBlockedScreen g_GameBlockedScreen;

CGameBlockedScreen::CGameBlockedScreen()
    : CBaseScreen(m_GameBlockedScreenGump)
{
}

CGameBlockedScreen::~CGameBlockedScreen()
{
}
 
void CGameBlockedScreen::Init()
{
    Code = 0;
}

void CGameBlockedScreen::Render(bool mode)
{
    DEBUG_TRACE_FUNCTION;
    if (mode)
    {
        g_GumpManager.Draw(true);

        InitToolTip();

        g_MouseManager.Draw(0x2073); //Main Gump mouse cursor
    }
    else
    {
        g_SelectedObject.Clear();

        g_GumpManager.Select(true);

        if (g_SelectedObject.Object != g_LastSelectedObject.Object)
        {
            if (g_SelectedObject.Object != NULL)
                g_SelectedObject.Object->OnMouseEnter();

            if (g_LastSelectedObject.Object != NULL)
                g_LastSelectedObject.Object->OnMouseExit();
        }

        g_LastSelectedObject.Init(g_SelectedObject);
    }
}

void CGameBlockedScreen::OnLeftMouseButtonDown()
{
    DEBUG_TRACE_FUNCTION;
    if (g_SelectedObject.Gump != NULL)
        g_GumpManager.OnLeftMouseButtonDown(true);
}

void CGameBlockedScreen::OnLeftMouseButtonUp()
{
    DEBUG_TRACE_FUNCTION;
    if (g_PressedObject.LeftGump != NULL)
        g_GumpManager.OnLeftMouseButtonUp(true);
}

#if USE_WISP
void CGameBlockedScreen::OnCharPress(const WPARAM &wParam, const LPARAM &lParam)
{
    DEBUG_TRACE_FUNCTION;
    if (g_EntryPointer == NULL || g_EntryPointer == &g_GameConsole)
        return;

    g_GumpManager.OnCharPress(wParam, lParam, true);
}
#else
void CGameBlockedScreen::OnTextInput(const SDL_TextInputEvent &ev)
{
    NOT_IMPLEMENTED; // FIXME
}
#endif

void CGameBlockedScreen::OnKeyDown(const KeyEvent &ev)
{
    DEBUG_TRACE_FUNCTION;

    CGumpNotify *notify = (CGumpNotify *)g_GumpManager.GetGump(0, 0, GT_NOTIFY);
    const auto key = EvKey(ev);
    if (g_EntryPointer == nullptr || g_EntryPointer == &g_GameConsole)
    {
        if (key == KEY_RETURN && notify != nullptr)
            notify->OnKeyDown(ev);
    }
    else
    {
        CGump *gump = g_GumpManager.GetTextEntryOwner();

        if (gump != nullptr && gump->GumpType == GT_TEXT_ENTRY_DIALOG)
            gump->OnKeyDown(ev);
        else if (notify != nullptr)
            notify->OnKeyDown(ev);
    }
}

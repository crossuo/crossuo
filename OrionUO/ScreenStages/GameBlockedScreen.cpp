// MIT License
// Copyright (C) August 2016 Hotride

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

void CGameBlockedScreen::Render()
{
    DEBUG_TRACE_FUNCTION;
    g_GumpManager.Draw(true);
    InitToolTip();
    g_MouseManager.Draw(0x2073); //Main Gump mouse cursor
}

void CGameBlockedScreen::SelectObject()
{
    DEBUG_TRACE_FUNCTION;

    g_SelectedObject.Clear();
    g_GumpManager.Select(true);
    if (g_SelectedObject.Object != g_LastSelectedObject.Object)
    {
        if (g_SelectedObject.Object != nullptr)
        {
            g_SelectedObject.Object->OnMouseEnter();
        }

        if (g_LastSelectedObject.Object != nullptr)
        {
            g_LastSelectedObject.Object->OnMouseExit();
        }
    }

    g_LastSelectedObject.Init(g_SelectedObject);
}

void CGameBlockedScreen::OnLeftMouseButtonDown()
{
    DEBUG_TRACE_FUNCTION;
    if (g_SelectedObject.Gump != nullptr)
    {
        g_GumpManager.OnLeftMouseButtonDown(true);
    }
}

void CGameBlockedScreen::OnLeftMouseButtonUp()
{
    DEBUG_TRACE_FUNCTION;
    if (g_PressedObject.LeftGump != nullptr)
    {
        g_GumpManager.OnLeftMouseButtonUp(true);
    }
}

void CGameBlockedScreen::OnTextInput(const TextEvent &ev)
{
    DEBUG_TRACE_FUNCTION;

    if (g_EntryPointer == nullptr || g_EntryPointer == &g_GameConsole)
    {
        return;
    }

    g_GumpManager.OnTextInput(ev, true);
}

void CGameBlockedScreen::OnKeyDown(const KeyEvent &ev)
{
    DEBUG_TRACE_FUNCTION;

    CGumpNotify *notify = (CGumpNotify *)g_GumpManager.GetGump(0, 0, GT_NOTIFY);
    const auto key = EvKey(ev);
    if (g_EntryPointer == nullptr || g_EntryPointer == &g_GameConsole)
    {
        if ((key == KEY_RETURN || key == KEY_RETURN2) && notify != nullptr)
        {
            notify->OnKeyDown(ev);
        }
    }
    else
    {
        CGump *gump = g_GumpManager.GetTextEntryOwner();
        if (gump != nullptr && gump->GumpType == GT_TEXT_ENTRY_DIALOG)
        {
            gump->OnKeyDown(ev);
        }
        else if (notify != nullptr)
        {
            notify->OnKeyDown(ev);
        }
    }
}

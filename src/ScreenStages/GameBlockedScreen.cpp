// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GameBlockedScreen.h"
#include "../PressedObject.h"
#include "../SelectedObject.h"
#include "../TextEngine/GameConsole.h"
#include "../Managers/GumpManager.h"
#include "../Managers/MouseManager.h"
#include "../Gumps/GumpNotify.h"

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
    CBaseScreen::Init();

    Code = 0;
}

void CGameBlockedScreen::Render()
{
    Render_ResetCmdList(g_renderCmdList, Render_DefaultState());
    RenderAdd_FlushState(g_renderCmdList);

    g_GumpManager.Draw(true);
    InitToolTip();
    g_MouseManager.Draw(0x2073); //Main Gump mouse cursor

    RenderDraw_Execute(g_renderCmdList);
}

void CGameBlockedScreen::SelectObject()
{
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
    if (g_SelectedObject.Gump != nullptr)
    {
        g_GumpManager.OnLeftMouseButtonDown(true);
    }
}

void CGameBlockedScreen::OnLeftMouseButtonUp()
{
    if (g_PressedObject.LeftGump != nullptr)
    {
        g_GumpManager.OnLeftMouseButtonUp(true);
    }
}

void CGameBlockedScreen::OnTextInput(const TextEvent &ev)
{
    if (g_EntryPointer == nullptr || g_EntryPointer == &g_GameConsole)
    {
        return;
    }

    g_GumpManager.OnTextInput(ev, true);
}

void CGameBlockedScreen::OnKeyDown(const KeyEvent &ev)
{
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

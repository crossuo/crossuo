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
#if defined(USE_GL3) || defined(USE_GLES)
    // this screen renders MID-frame (inside CGameScreen::Render), so the pool
    // clear must come first: it submits the still-pending batch (console/gump
    // text from earlier in this frame) under the state it was recorded with,
    // and only then is the command list reset - the other order flushed the
    // batch after the reset (wrong state) or, worse, freed textures the batch
    // still referenced (bind of deleted name)
    g_FontManager.ClearTextSpritePool();
    Render_ResetCmdList(g_renderCmdList, Render_DefaultState());
    RenderAdd_FlushState(g_renderCmdList);
#endif // #if defined(USE_GL3) || defined(USE_GLES)
    g_GumpManager.Draw(true);
    InitToolTip();
    g_MouseManager.Draw(0x2073); //Main Gump mouse cursor
#if defined(USE_GL3) || defined(USE_GLES)
    RenderDebug_ProcessFrame(g_renderCmdList);
    RenderDraw_Execute(g_renderCmdList);
    g_FontManager.ClearTextSpritePool(); // Clear text sprites (GPU is done with them)
#endif // #if defined(USE_GL3) || defined(USE_GLES)
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

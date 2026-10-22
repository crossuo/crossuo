// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GumpScreenGame.h"
#include "GUI/GUIButton.h"
#include "GumpConsoleType.h"
#include "GumpOptions.h"
#include "../Weather.h"
#include "../CrossUO.h"
#include "../ToolTip.h"
#include "../PressedObject.h"
#include "../SelectedObject.h"
#include "../GameWindow.h"
#include "../Managers/GumpManager.h"
#include "../Managers/ConfigManager.h"
#include "../Managers/MouseManager.h"
#include "../ScreenStages/GameScreen.h"
#include "../Network/Packets.h"
#include "../Utility/PerfMarker.h"
#include "xuocore/enumlist.h"

CGumpScreenGame::CGumpScreenGame()
    : CGump(GT_NONE, 0, 0, 0)
{
    NoMove = true;
    NoClose = true;

    Add(new CGUIButton(ID_GS_RESIZE, 0x0837, 0x0837, 0x0838, 0, 0));
    Add(new CGUIGumppic(0x0E14, 0, 0));
}

CGumpScreenGame::~CGumpScreenGame()
= default;

// web on a phone held upright: the maximized page is the display, the
// world fills it and there is no frame or resizer to operate. A narrow
// desktop browser window is NOT this (WebIsMobile)
static bool IsPortraitCanvas()
{
#if defined(XUO_EMSC)
    if (!WebIsFillViewport() || !WebIsMobile())
    {
        return false;
    }
    const CSize size = g_GameWindow.GetSize();
    return size.Width < size.Height;
#else
    return false;
#endif
}

void CGumpScreenGame::ClampToDisplay(int &x, int &y, int &w, int &h)
{
    // the rect is clamped against the display the game window lives in:
    // - native: the monitor minus the desktop menu-bar margins
    // - web: the canvas. SDL's "display" on web is the physical monitor
    //   (screen.width/height), which a maximized browser EXCEEDS, so using
    //   it here shaved 20px off a full-page canvas
    int screenX, screenY;
    GetDisplaySize(&screenX, &screenY);
    screenX -= 20;
    screenY -= 60;

    const CSize canvas = g_GameWindow.GetSize();
#if defined(XUO_EMSC)
    screenX = canvas.Width;
    screenY = canvas.Height;
#endif

    if (!WebIsMobile())
    {
        // the authored floor; a phone in portrait is narrower than 640
        if (w < 640)
        {
            w = 640;
        }

        if (h < 480)
        {
            h = 480;
        }
    }

    if (w >= screenX)
    {
        w = screenX;
    }

    if (h >= screenY)
    {
        h = screenY;
    }

    if (x < 0)
    {
        x = 0;
    }

    if (y < 0)
    {
        y = 0;
    }

    if (x + w > canvas.Width)
    {
        x = canvas.Width - w;
    }

    if (y + h > canvas.Height)
    {
        y = canvas.Height - h;
    }
}

void CGumpScreenGame::FitCanvas()
{
    const CSize size = g_GameWindow.GetSize();
    if (size.Width == m_LastCanvasWidth && size.Height == m_LastCanvasHeight)
    {
        return; // the screen gump updates every frame, the canvas rarely
    }
    m_LastCanvasWidth = size.Width;
    m_LastCanvasHeight = size.Height;

    if (!IsPortraitCanvas())
    {
        return;
    }

    // rotating the phone keeps the world edge to edge; the render rect only,
    // the desktop config comes back in landscape
    g_RenderBounds.GameWindowPosX = 0;
    g_RenderBounds.GameWindowPosY = 0;
    g_RenderBounds.GameWindowWidth = size.Width;
    g_RenderBounds.GameWindowHeight = size.Height;

    // world zoom (pinch) renders through g_GlobalScale only when the desktop
    // UseScaling option is on; touch keeps it on
    if (!g_ConfigManager.GetUseScaling())
    {
        g_ConfigManager.SetUseScaling(true);
    }

#if defined(XUO_EMSC)
    Info(
        Client,
        "touch: portrait game window fills canvas %dx%d",
        size.Width,
        size.Height);
#endif
}

void CGumpScreenGame::UpdateContent()
{
    FitCanvas();

#if defined(XUO_EMSC)
    if (WebIsFillViewport() && !WebIsMobile())
    {
        // web desktop: a saved rect that covers the whole canvas was
        // written by the mobile fill era (the rect was forced to the
        // canvas and then persisted) - its border sits outside the canvas
        // and the bar reads as fullscreen. Restore the default windowed
        // rect; a real user drag never reaches full bleed (the clamp keeps
        // the resizer reachable inside the canvas)
        const CSize canvas = g_GameWindow.GetSize();
        if (g_ConfigManager.GameWindowWidth >= canvas.Width &&
            g_ConfigManager.GameWindowHeight >= canvas.Height)
        {
            g_ConfigManager.GameWindowX = 0;
            g_ConfigManager.GameWindowY = 40; // below the menubar
            g_ConfigManager.GameWindowWidth = canvas.Width < 800 ? canvas.Width : 800;
            g_ConfigManager.GameWindowHeight = canvas.Height < 600 ? canvas.Height : 600;
            Info(Client, "web desktop: stale full-bleed rect reset to 800x600");
        }
    }
#endif

#if defined(XUO_EMSC)
    // web: the canvas is the display, and the saved per-account rect can
    // predate it (a config from another device or from before a resize);
    // the world must never render outside it. Native keeps the authored
    // behavior: limits apply while the resizer is dragged, nothing else
    ClampToDisplay(
        g_RenderBounds.GameWindowPosX,
        g_RenderBounds.GameWindowPosY,
        g_RenderBounds.GameWindowWidth,
        g_RenderBounds.GameWindowHeight);
#endif

    if (g_PressedObject.LeftGump == this)
    {
        CPoint2Di offset = g_MouseManager.LeftDroppedOffset();

        if (g_PressedObject.LeftObject == m_Items) //resizer
        {
            g_RenderBounds.GameWindowWidth += offset.X;
            g_RenderBounds.GameWindowHeight += offset.Y;

            ClampToDisplay(
                g_RenderBounds.GameWindowPosX,
                g_RenderBounds.GameWindowPosY,
                g_RenderBounds.GameWindowWidth,
                g_RenderBounds.GameWindowHeight);

            CGumpOptions *opt = (CGumpOptions *)g_GumpManager.UpdateGump(0, 0, GT_OPTIONS);

            if (opt != nullptr)
            {
                opt->m_GameWindowWidth->m_Entry.SetTextW(wstr_from(g_RenderBounds.GameWindowWidth));
                opt->m_GameWindowHeight->m_Entry.SetTextW(
                    wstr_from(g_RenderBounds.GameWindowHeight));
            }
        }
        else //scope
        {
            g_RenderBounds.GameWindowPosX += offset.X;
            g_RenderBounds.GameWindowPosY += offset.Y;

            ClampToDisplay(
                g_RenderBounds.GameWindowPosX,
                g_RenderBounds.GameWindowPosY,
                g_RenderBounds.GameWindowWidth,
                g_RenderBounds.GameWindowHeight);
        }
    }
}

void CGumpScreenGame::InitToolTip()
{
    if (!g_ConfigManager.UseToolTips || g_SelectedObject.Object == nullptr)
    {
        return;
    }

    if (g_SelectedObject.Serial == ID_GS_RESIZE && !IsPortraitCanvas())
    {
        g_ToolTip.Set(L"Resize game window", 100);
    }
}

void CGumpScreenGame::Draw()
{
    ScopedPerfMarker(__FUNCTION__);

    if (IsPortraitCanvas())
    {
        // the world owns the phone screen: no frame, no resizer
        return;
    }

    // Game window frame
    g_Game.DrawGump(
        0x0A8D,
        0,
        g_RenderBounds.GameWindowPosX - 4,
        g_RenderBounds.GameWindowPosY - 4,
        0,
        g_RenderBounds.GameWindowHeight + 8);
    g_Game.DrawGump(
        0x0A8D,
        0,
        g_RenderBounds.GameWindowPosX + g_RenderBounds.GameWindowWidth,
        g_RenderBounds.GameWindowPosY - 4,
        0,
        g_RenderBounds.GameWindowHeight + 8);

    g_Game.DrawGump(
        0x0A8C,
        0,
        g_RenderBounds.GameWindowPosX - 4,
        g_RenderBounds.GameWindowPosY - 4,
        g_RenderBounds.GameWindowWidth + 4,
        0);
    g_Game.DrawGump(
        0x0A8C,
        0,
        g_RenderBounds.GameWindowPosX - 4,
        g_RenderBounds.GameWindowPosY + g_RenderBounds.GameWindowHeight,
        g_RenderBounds.GameWindowWidth + 8,
        0);

    uint16_t resizeGumpID = 0x0837; //button
    if (g_ConfigManager.LockResizingGameWindow)
    {
        resizeGumpID = 0x082C; //lock
    }
    else if (g_SelectedObject.Object == m_Items)
    {
        resizeGumpID++; //lighted button
    }

    g_Game.DrawGump(
        resizeGumpID,
        0,
        g_RenderBounds.GameWindowPosX + g_RenderBounds.GameWindowWidth - 3,
        g_RenderBounds.GameWindowPosY + g_RenderBounds.GameWindowHeight - 3);
}

CRenderObject *CGumpScreenGame::Select()
{
    CRenderObject *selected = nullptr;

    if (!g_ConfigManager.LockResizingGameWindow && !IsPortraitCanvas())
    {
        if (g_Game.GumpPixelsInXY(
                0x0837,
                g_RenderBounds.GameWindowPosX + g_RenderBounds.GameWindowWidth - 3,
                g_RenderBounds.GameWindowPosY + g_RenderBounds.GameWindowHeight - 3))
        {
            selected = (CRenderObject *)m_Items;
        }
        else if (
            g_Game.GumpPixelsInXY(
                0x0A8D,
                g_RenderBounds.GameWindowPosX - 4,
                g_RenderBounds.GameWindowPosY - 4,
                0,
                g_RenderBounds.GameWindowHeight + 8))
        {
            selected = (CRenderObject *)m_Items->m_Next;
        }
        else if (
            g_Game.GumpPixelsInXY(
                0x0A8D,
                g_RenderBounds.GameWindowPosX + g_RenderBounds.GameWindowWidth,
                g_RenderBounds.GameWindowPosY - 4,
                0,
                g_RenderBounds.GameWindowHeight + 8))
        {
            selected = (CRenderObject *)m_Items->m_Next;
        }
        else if (
            g_Game.GumpPixelsInXY(
                0x0A8C,
                g_RenderBounds.GameWindowPosX - 4,
                g_RenderBounds.GameWindowPosY - 4,
                g_RenderBounds.GameWindowWidth + 8,
                0))
        {
            selected = (CRenderObject *)m_Items->m_Next;
        }
        else if (
            g_Game.GumpPixelsInXY(
                0x0A8C,
                g_RenderBounds.GameWindowPosX - 4,
                g_RenderBounds.GameWindowPosY + g_RenderBounds.GameWindowHeight,
                g_RenderBounds.GameWindowWidth + 8,
                0))
        {
            selected = (CRenderObject *)m_Items->m_Next;
        }

        if (selected != nullptr)
        {
            g_SelectedObject.Init(selected, this);
        }
    }

    return selected;
}

void CGumpScreenGame::OnLeftMouseButtonDown()
{
    //CGump::OnLeftMouseButtonDown();

    if (g_GumpConsoleType != nullptr)
    {
        g_GumpManager.MoveToBack(g_GumpConsoleType);
    }
}

void CGumpScreenGame::OnLeftMouseButtonUp()
{
    if (IsPortraitCanvas())
    {
        return;
    }

    CPoint2Di offset = g_MouseManager.LeftDroppedOffset();

    if (g_PressedObject.LeftObject == m_Items) //resizer
    {
        g_ConfigManager.GameWindowWidth = g_ConfigManager.GameWindowWidth + offset.X;
        g_ConfigManager.GameWindowHeight = g_ConfigManager.GameWindowHeight + offset.Y;

        ClampToDisplay(
            g_ConfigManager.GameWindowX,
            g_ConfigManager.GameWindowY,
            g_ConfigManager.GameWindowWidth,
            g_ConfigManager.GameWindowHeight);

        g_Weather.Generate();

        if (g_Config.ClientVersion >= CV_200)
        {
            CPacketGameWindowSize().Send();
        }
    }
    else //scope
    {
        g_ConfigManager.GameWindowX = g_ConfigManager.GameWindowX + offset.X;
        g_ConfigManager.GameWindowY = g_ConfigManager.GameWindowY + offset.Y;

        if (g_ConfigManager.GameWindowX < 1)
        {
            g_ConfigManager.GameWindowX = 0;
        }

        if (g_ConfigManager.GameWindowY < 1)
        {
            g_ConfigManager.GameWindowY = 0;
        }

        if (g_ConfigManager.GameWindowX + g_ConfigManager.GameWindowWidth >
            g_GameWindow.GetSize().Width)
        {
            g_ConfigManager.GameWindowX =
                g_GameWindow.GetSize().Width - g_ConfigManager.GameWindowWidth;
        }

        if (g_ConfigManager.GameWindowY + g_ConfigManager.GameWindowHeight >
            g_GameWindow.GetSize().Height)
        {
            g_ConfigManager.GameWindowY =
                g_GameWindow.GetSize().Height - g_ConfigManager.GameWindowHeight;
        }
    }
}

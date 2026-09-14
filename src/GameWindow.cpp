// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include <SDL_timer.h>
#include <common/utils.h>
#include "GameWindow.h"
#include "CrossUO.h"
#include "ServerList.h"
#include "PressedObject.h"
#include "SelectedObject.h"
#include "ScreenshotBuilder.h"
#include "ClickObject.h"
#include "Target.h"
#include "Managers/ConfigManager.h"
#include "Managers/ScreenEffectManager.h"
#include "Managers/SoundManager.h"
#include "Managers/GumpManager.h"
#include "Managers/PacketManager.h"
#include "Managers/ConnectionManager.h"
#include "Managers/MouseManager.h"
#include "Network/Packets.h"
#include "ScreenStages/MainScreen.h"
#include "ScreenStages/ServerScreen.h"
#include "Gumps/Gump.h"
#include "Walker/PathFinder.h"
#include "Splash.h"

#if !defined(NEW_RENDERER_ENABLED) || defined(RENDERER_LEGACY)
#include "GLEngine/GLEngine.h"
#endif // #if !defined(NEW_RENDERER_ENABLED) || defined(RENDERER_LEGACY)

#if USE_PING
#include "Utility/PingThread.h"
#endif // USE_PING

CGameWindow g_GameWindow;

CGameWindow::CGameWindow()
    : m_iRenderDelay(0)
{
}

CGameWindow::~CGameWindow()
{
}

void CGameWindow::SetRenderTimerDelay(int delay)
{
    m_iRenderDelay = delay;
}

bool CGameWindow::OnCreate()
{
#if !defined(NEW_RENDERER_ENABLED)
    if (!g_GL.Install())
#else
    if (!Render_Init(m_window))
#endif
    {
        Error(Client, "error initializing OpenGL");
        ShowMessage("Error initializing OpenGL", "Error");
        return false;
    }

#if defined(NEW_RENDERER_ENABLED)
    HACKRender_SetViewParams(SetViewParamsCmd{
        0, 0, m_Size.Width, m_Size.Height, m_Size.Width, m_Size.Height, -150, 150 });
#endif

    // Present the embedded splash image as soon as there is a GL context: it
    // covers the window while the data files are loaded and the first screen is
    // built, and the presented frame stays on screen for as long as the loading
    // code below does not swap.
    if (Splash_Init())
    {
        Splash_Render();
        Info(Client, "splash screen displayed");
    }

    if (!g_Game.Install())
    {
        return false;
    }

#if !defined(NEW_RENDERER_ENABLED)
    g_GL.UpdateRect();
#endif
    g_GumpManager.RedrawAll();
    return true;
}

void CGameWindow::OnDestroy()
{
    g_SoundManager.Free();
    g_Game.Uninstall();
}

void CGameWindow::OnResize()
{
#if !defined(NEW_RENDERER_ENABLED)
    g_GL.UpdateRect();
#else
    RenderAdd_SetViewParams(
        g_renderCmdList,
        SetViewParamsCmd{
            0, 0, m_Size.Width, m_Size.Height, m_Size.Width, m_Size.Height, -150, 150 });
#endif
    g_GumpManager.RedrawAll();
}

void CGameWindow::EmulateOnLeftMouseButtonDown()
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->SelectObject();
        g_PressedObject.InitLeft(g_SelectedObject);
        if (g_SelectedObject.Object != nullptr || g_GameState == GS_GAME)
        {
            Wisp::g_WispMouse->LeftDropPosition = Wisp::g_WispMouse->Position;
            g_CurrentScreen->OnLeftMouseButtonDown();
        }
    }
}

int CGameWindow::GetRenderDelay()
{
    return m_iRenderDelay;
}

void CGameWindow::OnLeftMouseButtonDown()
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_GeneratedMouseDown = false;
        if (g_Target.MultiGraphic == 0)
        {
            g_CurrentScreen->SelectObject();
        }
        g_PressedObject.InitLeft(g_SelectedObject);
        if (g_SelectedObject.Object != nullptr || g_GameState == GS_GAME)
        {
            g_CurrentScreen->OnLeftMouseButtonDown();
        }
    }
}

void CGameWindow::OnLeftMouseButtonUp()
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        if (g_Target.MultiGraphic == 0)
        {
            g_CurrentScreen->SelectObject();
        }
        //if ((g_SelectedObject.Object() != nullptr && g_SelectedObject.Object() == g_PressedObject.LeftObject && g_SelectedObject.Serial) || g_GameState >= GS_GAME)
        if ((g_SelectedObject.Object != nullptr && (g_SelectedObject.Serial != 0u)) ||
            g_GameState >= GS_GAME)
        {
            g_CurrentScreen->OnLeftMouseButtonUp();
            if (g_MovingFromMouse && g_PressedObject.LeftGump == nullptr)
            {
                g_AutoMoving = true;
            }
        }

        if (g_PressedObject.LeftObject != nullptr && g_PressedObject.LeftObject->IsGUI() &&
            ((CBaseGUI *)g_PressedObject.LeftObject)->Type == GOT_COMBOBOX &&
            g_PressedObject.LeftGump != nullptr)
        {
            g_PressedObject.LeftGump->WantRedraw = true;
        }

        g_PressedObject.ClearLeft();
    }
}

bool CGameWindow::OnLeftMouseButtonDoubleClick()
{
    bool result = false;

    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->SelectObject();
        g_PressedObject.InitLeft(g_SelectedObject);
        result = (g_SelectedObject.Object != nullptr &&
                  g_SelectedObject.Object == g_PressedObject.LeftObject) &&
                 g_CurrentScreen->OnLeftMouseButtonDoubleClick();

        if (result)
        {
            g_PressedObject.ClearLeft();
            g_ClickObject.Clear();
        }
    }

    return result;
}

void CGameWindow::OnRightMouseButtonDown()
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        if (g_Target.MultiGraphic == 0)
        {
            g_CurrentScreen->SelectObject();
        }
        g_PressedObject.InitRight(g_SelectedObject);
        g_CurrentScreen->OnRightMouseButtonDown();
        if (g_SelectedObject.Gump == nullptr &&
            !(g_MouseManager.Position.X < g_ConfigManager.GameWindowX ||
              g_MouseManager.Position.Y < g_ConfigManager.GameWindowY ||
              g_MouseManager.Position.X >
                  (g_ConfigManager.GameWindowX + g_ConfigManager.GameWindowWidth) ||
              g_MouseManager.Position.Y >
                  (g_ConfigManager.GameWindowY + g_ConfigManager.GameWindowHeight)))
        {
            g_MovingFromMouse = true;
            g_AutoMoving = false;
        }
    }
}

void CGameWindow::OnRightMouseButtonUp()
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        if (g_Target.MultiGraphic == 0)
        {
            g_CurrentScreen->SelectObject();
        }
        if ((g_SelectedObject.Object != nullptr &&
             g_SelectedObject.Object == g_PressedObject.RightObject &&
             (g_SelectedObject.Serial != 0u)) ||
            g_GameState >= GS_GAME)
        {
            g_CurrentScreen->OnRightMouseButtonUp();
        }
        g_MovingFromMouse = false;
        g_PressedObject.ClearRight();
    }
}

bool CGameWindow::OnRightMouseButtonDoubleClick()
{
    bool result = false;
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->SelectObject();
        g_PressedObject.InitRight(g_SelectedObject);
        result = (g_SelectedObject.Object != nullptr &&
                  g_SelectedObject.Object == g_PressedObject.RightObject) &&
                 g_CurrentScreen->OnRightMouseButtonDoubleClick();

        if (result)
        {
            g_PressedObject.ClearRight();
        }
    }

    return result;
}

void CGameWindow::OnMidMouseButtonDown()
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->OnMidMouseButtonDown();
    }
}

void CGameWindow::OnMidMouseButtonUp()
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->OnMidMouseButtonUp();
    }
}

bool CGameWindow::OnMidMouseButtonDoubleClick()
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        return g_CurrentScreen->OnMidMouseButtonDoubleClick();
    }

    return false;
}

void CGameWindow::OnMidMouseButtonScroll(bool up)
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->SelectObject();
        g_CurrentScreen->OnMidMouseButtonScroll(up);
    }
}

void CGameWindow::OnDragging()
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->OnDragging();
    }
}

void CGameWindow::OnActivate()
{
    g_Game.ResumeSound();
    SetRenderTimerDelay(g_FrameDelay[WINDOW_ACTIVE]);
}

void CGameWindow::OnDeactivate()
{
    if (!g_ConfigManager.BackgroundSound)
    {
        g_Game.PauseSound();
    }

    if (g_ConfigManager.GetReduceFPSUnactiveWindow())
    {
        SetRenderTimerDelay(g_FrameDelay[WINDOW_INACTIVE]);
    }
}

void CGameWindow::OnTextInput(const TextEvent &ev)
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->OnTextInput(ev);
    }
}

void CGameWindow::OnKeyDown(const KeyEvent &ev)
{
    const auto key = EvKey(ev);
    const bool acceptKey = true;
    if (acceptKey && g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
#if __APPLE__
#define CMD_KEY g_CmdPressed
#else
#define CMD_KEY g_CtrlPressed
#endif
        if (CMD_KEY && key == SDLK_v && g_EntryPointer != nullptr)
        {
            if (g_GameState == GS_MAIN)
            {
                g_MainScreen.Paste();
            }
            else
            {
                g_EntryPointer->Paste();
            }
        }

        g_CurrentScreen->OnKeyDown(ev);
    }
}

void CGameWindow::OnKeyUp(const KeyEvent &ev)
{
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->OnKeyUp(ev);
    }

    const auto key = EvKey(ev);
    if (key == KEY_PRINTSCREEN)
    {
        g_ScreenshotBuilder.SaveScreen();
    }
}

void CGameWindow::OnTimer(uint32_t id)
{
    if (id == FASTLOGIN_TIMER_ID)
    {
        RemoveTimer(FASTLOGIN_TIMER_ID);
        g_Game.Connect();
    }
}

bool CGameWindow::OnUserMessages(const UserEvent &ev)
{
    switch (ev.code)
    {
#if USE_PING
        case CPingThread::MessageID:
        {
            PING_INFO_DATA *info = (PING_INFO_DATA *)ev.data1;
            if (info != nullptr)
            {
                if (info->ServerID == 0xFFFFFFFF)
                {
                    memcpy(&g_GameServerPingInfo, info, sizeof(g_GameServerPingInfo));
                    char ping[50] = { 0 };
                    sprintf_s(
                        ping,
                        "ping(min:%i max:%i avg:%i lost:%i) ",
                        g_GameServerPingInfo.Min,
                        g_GameServerPingInfo.Max,
                        g_GameServerPingInfo.Average,
                        g_GameServerPingInfo.Lost);
                    g_PingString = ping;
                }
                else
                {
                    CServer *server = g_ServerList.GetServer(info->ServerID);
                    if (server != nullptr)
                    {
                        if (info->Min < 9999)
                        {
                            server->Ping = info->Average;
                            server->PacketsLoss = info->Lost;
                            g_ServerScreen.UpdateContent();
                        }
                    }
                }

                Info(
                    Client,
                    "Ping info: id:%i min:%i max:%i average:%i lost:%i",
                    info->ServerID,
                    info->Min,
                    info->Max,
                    info->Average,
                    info->Lost);
                delete info;
            }
        }
        break;
#endif // USE_PING

        case CGameWindow::MessageID:
        {
            OnTimer(checked_cast<uint32_t>(ev.data1));
            break;
        }

        default:
        {
        }
        break;
    }

    return true;
}

void CGameWindow::SetWindowResizable(const bool windowResizable) const
{
#if SDL_VERSION_ATLEAST(2, 0, 5)
    SDL_SetWindowResizable(m_window, windowResizable ? SDL_TRUE : SDL_FALSE);
#endif
}

void CGameWindow::RestoreWindow() const
{
    SDL_RestoreWindow(m_window);
}

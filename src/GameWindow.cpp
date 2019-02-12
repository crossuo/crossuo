// MIT License
// Copyright (C) August 2016 Hotride

#include <SDL_timer.h>
#include "GameWindow.h"
#include "CrossUO.h"
#include "ServerList.h"
#include "PressedObject.h"
#include "SelectedObject.h"
#include "ScreenshotBuilder.h"
#include "ClickObject.h"
#include "Managers/ConfigManager.h"
#include "Managers/PluginManager.h"
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

#if USE_PING
#include "Utility/PingThread.h"
#endif // USE_PING

CGameWindow g_GameWindow;

CGameWindow::CGameWindow()
    : m_iRenderDelay(0)
{
#if 0
    // Add some tests, this cliloc message seems to use broken UTF8
    // "Hey buddy... Looking for work?"
    //            ^^- c2 a0 - is an invalid code point
    uint8_t d[] = {
        0x48, 0x65, 0x79, 0x20, 0x62, 0x75, 0x64, 0x64, 0x79, 0x2E, 0xC2,
        0xA0, 0x20, 0x4C, 0x6F, 0x6F, 0x6B, 0x69, 0x6E, 0x67, 0x20, 0x66,
        0x6F, 0x72, 0x20, 0x77, 0x6F, 0x72, 0x6B, 0x3F
    };
    string s( reinterpret_cast< char const* >( d ) ) ;
    DecodeUTF8(s);
#endif
}

CGameWindow::~CGameWindow()
{
}

void CGameWindow::SetRenderTimerDelay(int delay)
{
    DEBUG_TRACE_FUNCTION;
    m_iRenderDelay = delay;
}

bool CGameWindow::OnCreate()
{
    DEBUG_TRACE_FUNCTION;
    if (!g_GL.Install())
    {
        Info(Client, "error initializing OpenGL");
        ShowMessage("Error initializing OpenGL", "Error");
        return false;
    }

    if (!g_Game.Install())
    {
        return false;
    }

    g_GL.UpdateRect();
    return true;
}

void CGameWindow::OnDestroy()
{
    DEBUG_TRACE_FUNCTION;
    g_SoundManager.Free();
    PLUGIN_EVENT(UOMSG_WIN_CLOSE, nullptr);
    g_Game.Uninstall();
}

void CGameWindow::OnResize()
{
    DEBUG_TRACE_FUNCTION;
    g_GL.UpdateRect();
}

void CGameWindow::EmulateOnLeftMouseButtonDown()
{
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_GeneratedMouseDown = false;
        g_CurrentScreen->SelectObject();
        g_PressedObject.InitLeft(g_SelectedObject);
        if (g_SelectedObject.Object != nullptr || g_GameState == GS_GAME)
        {
            g_CurrentScreen->OnLeftMouseButtonDown();
        }
    }
}

void CGameWindow::OnLeftMouseButtonUp()
{
    DEBUG_TRACE_FUNCTION;
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->SelectObject();

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
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->SelectObject();
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
    DEBUG_TRACE_FUNCTION;
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->SelectObject();
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
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
    if (PLUGIN_EVENT(UOMSG_INPUT_MBUTTONDOWN, 0x11110000))
    {
        return;
    }

    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->OnMidMouseButtonDown();
    }
}

void CGameWindow::OnMidMouseButtonUp()
{
    DEBUG_TRACE_FUNCTION;
    if (PLUGIN_EVENT(UOMSG_INPUT_MBUTTONDOWN, 0))
    {
        return;
    }

    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->OnMidMouseButtonUp();
    }
}

bool CGameWindow::OnMidMouseButtonDoubleClick()
{
    DEBUG_TRACE_FUNCTION;
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        return g_CurrentScreen->OnMidMouseButtonDoubleClick();
    }

    return false;
}

void CGameWindow::OnMidMouseButtonScroll(bool up)
{
    DEBUG_TRACE_FUNCTION;
    if (PLUGIN_EVENT(UOMSG_INPUT_MOUSEWHEEL, up ? 0 : 0x11110000))
    {
        return;
    }

    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->SelectObject();
        g_CurrentScreen->OnMidMouseButtonScroll(up);
    }
}

void CGameWindow::OnXMouseButton(bool up)
{
    DEBUG_TRACE_FUNCTION;
    if (PLUGIN_EVENT(UOMSG_INPUT_XBUTTONDOWN, up ? 0 : 0x11110000))
    {
        return;
    }
}

void CGameWindow::OnDragging()
{
    DEBUG_TRACE_FUNCTION;
    if (g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->OnDragging();
    }
}

void CGameWindow::OnActivate()
{
    DEBUG_TRACE_FUNCTION;
    g_Game.ResumeSound();
    SetRenderTimerDelay(g_FrameDelay[WINDOW_ACTIVE]);
    if (!g_PluginManager.Empty())
    {
        PLUGIN_EVENT(UOMSG_WIN_ACTIVATE, 1);
    }
}

void CGameWindow::OnDeactivate()
{
    DEBUG_TRACE_FUNCTION;
    if (!g_ConfigManager.BackgroundSound)
    {
        g_Game.PauseSound();
    }

    if (g_ConfigManager.GetReduceFPSUnactiveWindow())
    {
        SetRenderTimerDelay(g_FrameDelay[WINDOW_INACTIVE]);
    }

    if (!g_PluginManager.Empty())
    {
        PLUGIN_EVENT(UOMSG_WIN_ACTIVATE, 0);
    }
}

void CGameWindow::OnTextInput(const TextEvent &ev)
{
    DEBUG_TRACE_FUNCTION;

    const auto ch = EvChar(ev);
    if (PLUGIN_EVENT(UOMSG_INPUT_CHAR, &ev))
    {
        return;
    }

    if ((IsPrintable(ch) || (g_GameState >= GS_GAME && (ch == 0x11 || ch == 0x17))) &&
        g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->OnTextInput(ev);
    }
    else if (ch == 0x16 && g_EntryPointer != nullptr)
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
}

void CGameWindow::OnKeyDown(const KeyEvent &ev)
{
    DEBUG_TRACE_FUNCTION;

    if (PLUGIN_EVENT(UOMSG_INPUT_KEYDOWN, &ev))
    {
        return;
    }

    const auto key = EvKey(ev);
    const bool acceptKey = true;
    if (acceptKey && g_CurrentScreen != nullptr && g_ScreenEffectManager.Mode == SEM_NONE)
    {
        g_CurrentScreen->OnKeyDown(ev);
    }
}

void CGameWindow::OnKeyUp(const KeyEvent &ev)
{
    DEBUG_TRACE_FUNCTION;

    // FIXME: Send struct events to plugins
    if (PLUGIN_EVENT(UOMSG_INPUT_KEYUP, &ev))
    {
        return;
    }

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

bool CGameWindow::OnRepaint(const PaintEvent &ev)
{
    DEBUG_TRACE_FUNCTION;

    if (!g_PluginManager.Empty())
    {
        return PLUGIN_EVENT(UOMSG_WIN_PAINT, &ev) != 0u;
    }
    return false;
}

void CGameWindow::OnShow(bool show)
{
    DEBUG_TRACE_FUNCTION;
    if (!g_PluginManager.Empty())
    {
        PLUGIN_EVENT(UOMSG_WIN_SHOW, show);
    }
}

void CGameWindow::OnSetText(const char *str)
{
    DEBUG_TRACE_FUNCTION;
    if (!g_PluginManager.Empty())
    {
        // CHECK: str was second parameter, why?
        // Anyway, we're wilingly breaking compatibility with old stuff
        PLUGIN_EVENT(UOMSG_WIN_SETTEXT, str);
    }
}

void CGameWindow::OnTimer(uint32_t id)
{
    DEBUG_TRACE_FUNCTION;
    if (id == FASTLOGIN_TIMER_ID)
    {
        RemoveTimer(FASTLOGIN_TIMER_ID);
        g_Game.Connect();
    }
}

bool CGameWindow::OnUserMessages(const UserEvent &ev)
{
    DEBUG_TRACE_FUNCTION;

    switch (ev.code)
    {
        case UOMSG_RECV:
        {
            AutoFree p(ev.data1);

            g_PacketManager.SavePluginReceivePacket(
                (uint8_t *)ev.data1, checked_cast<int>(ev.data2));
            return true;
        }
        break;

        case UOMSG_SEND:
        {
            AutoFree p(ev.data1);

            uint32_t ticks = g_Ticks;
            uint8_t *buf = (uint8_t *)ev.data1;
            int size = checked_cast<int>(ev.data2);

            g_TotalSendSize += size;

            CPacketInfo &type = g_PacketManager.GetInfo(*buf);

            Info(
                Plugin,
                "--- ^(%d) s(+%d => %d) Plugin->Server:: %s",
                ticks - g_LastPacketTime,
                size,
                g_TotalSendSize,
                type.Name);

            g_LastPacketTime = ticks;
            g_LastSendTime = ticks;

            if (*buf == 0x80 || *buf == 0x91)
            {
                INFO_DUMP(Plugin, "SEND:", buf, 1);
                SAFE_DEBUG_DUMP(Plugin, "SEND:", buf, size);
                Info(Plugin, "**** ACCOUNT AND PASSWORD CENSORED ****");
            }
            else
            {
                INFO_DUMP(Plugin, "SEND:", buf, size);
            }
            g_ConnectionManager.Send((uint8_t *)ev.data1, checked_cast<int>(ev.data2));
            return true;
        }
        break;

        case UOMSG_PATHFINDING:
        {
            const auto xy = checked_cast<uint32_t>(ev.data1);
            const auto zd = checked_cast<uint32_t>(ev.data2);
            return !g_PathFinder.WalkTo(
                (xy >> 16) & 0xFFFF, xy & 0xFFFF, (zd >> 16) & 0xFFFF, zd & 0xFFFF);
        }
        break;

        case UOMSG_WALK:
        {
            const auto run = (ev.data1 != 0u);
            const auto dir = checked_cast<uint8_t>(ev.data2);
            return !g_PathFinder.Walk(run, dir);
        }
        break;

        case UOMSG_MENU_RESPONSE:
        {
            auto data = unique_cast<UOI_MENU_RESPONSE>(ev.data1);
            if ((data->Serial == 0u) && (data->ID == 0u))
            {
                for (CGump *gump = (CGump *)g_GumpManager.m_Items; gump != nullptr;)
                {
                    CGump *next = (CGump *)gump->m_Next;
                    if (gump->GumpType == GT_MENU || gump->GumpType == GT_GRAY_MENU)
                    {
                        CPacketMenuResponse(gump, data->Code).Send();
                        g_GumpManager.RemoveGump(gump);
                    }
                    gump = next;
                }
                break;
            }

            CGump *gump = g_GumpManager.GetGump(data->Serial, data->ID, GT_MENU);
            if (gump == nullptr)
            {
                gump = g_GumpManager.GetGump(data->Serial, data->ID, GT_GRAY_MENU);
                if (gump != nullptr)
                {
                    CPacketGrayMenuResponse(gump, data->Code).Send();
                    g_GumpManager.RemoveGump(gump);
                }
            }
            else
            {
                CPacketMenuResponse(gump, data->Code).Send();
                g_GumpManager.RemoveGump(gump);
            }
        }
        break;

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

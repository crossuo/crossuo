// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include <SDL_timer.h>
#include <common/utils.h> // countof
#include <external/gfx/gfx.h> // win_gfx_context_attrbutes
#include "icon_client.h"

#if defined(XUO_EMSC)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif


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
    if (!Render_Init(m_window))
    {
        Error(Client, "error initializing OpenGL");
        ShowMessage("Error initializing OpenGL", "Error");
        return false;
    }

    HACKRender_SetViewParams(SetViewParamsCmd{
        0, 0, m_Size.Width, m_Size.Height, m_Size.Width, m_Size.Height, -150, 150 });

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
    RenderAdd_SetViewParams(
        g_renderCmdList,
        SetViewParamsCmd{
            0, 0, m_Size.Width, m_Size.Height, m_Size.Width, m_Size.Height, -150, 150 });
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
            g_MouseManager.LeftDropPosition = g_MouseManager.Position;
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

#if defined(XUO_EMSC)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif



#if defined(XUO_EMSC)
// the browser document is the web "desktop": until the game maximizes, the
// canvas keeps the authored resolution (640x480, like the native default
// window); MaximizeWindow (CGameScreen::Init, on entering the world) flips
// the canvas to track the viewport, like a native window filling the screen
static bool s_webFillViewport = false;

static CSize web_viewport_size()
{
    // DOM access is main-thread only and main() runs on a pthread worker
    // (PROXY_TO_PTHREAD): run the JS on the main browser thread
    const int w = MAIN_THREAD_EM_ASM_INT({ return window.innerWidth; });
    const int h = MAIN_THREAD_EM_ASM_INT({ return window.innerHeight; });
    return CSize(w > 0 ? w : 640, h > 0 ? h : 480);
}

static EM_BOOL web_on_window_resize(int eventType, const EmscriptenUiEvent *event, void *userData)
{
    if (s_webFillViewport && g_GameWindow.m_window != nullptr)
    {
        g_GameWindow.SetSize(web_viewport_size());
    }
    return EM_FALSE;
}
#endif


void CGameWindow::SetMinSize(int width, int height)
{
    m_MinSize.Width = width;
    m_MinSize.Height = height;
}
void CGameWindow::SetMaxSize(int width, int height)
{
    m_MaxSize.Width = width;
    m_MaxSize.Height = height;
}

void CGameWindow::MaximizeWindow()
{
#if defined(XUO_EMSC)
    // native maximizes into the desktop; on web the desktop is the browser
    // document. SetSize drives SDL, which resizes the canvas drawing buffer
    // and sends the resize event the game already handles (same as native)
    s_webFillViewport = true;
    SetSize(web_viewport_size());
    return;
#endif
    SDL_RestoreWindow(m_window);
    SDL_MaximizeWindow(m_window);
}

void CGameWindow::SetSize(const CSize &size)
{
    SDL_SetWindowSize(m_window, size.Width, size.Height);
    m_Size = size;
}

void CGameWindow::SetPositionSize(int x, int y, int width, int height)
{
#if SDL_VERSION_ATLEAST(2, 0, 5)
    int borderH;
    SDL_GetWindowBordersSize(m_window, &borderH, nullptr, nullptr, nullptr);
#else
    int borderH = 0;
#endif
    SDL_RestoreWindow(m_window);
    SDL_SetWindowPosition(m_window, x, std::max(y, borderH));
    SDL_SetWindowSize(m_window, width, height);
}

void CGameWindow::GetPositionSize(int *x, int *y, int *width, int *height)
{
#if SDL_VERSION_ATLEAST(2, 0, 5)
    int borderH;
    SDL_GetWindowBordersSize(m_window, &borderH, nullptr, nullptr, nullptr);
#else
    int borderH = 0;
#endif
    SDL_GetWindowPosition(m_window, x, y);
    *y = std::max(*y, borderH);
    SDL_GetWindowSize(m_window, width, height);
}

bool CGameWindow::Create(const char *title, bool showCursor, int width, int height)
{
#ifdef OGL_DEBUGCONTEXT_ENABLED
    auto debugContext = true;
#else
    auto debugContext = false;
#endif
    if (debugContext)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    }
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
    {
        return false;
    }

    m_Size.Width = width;
    m_Size.Height = height;
    Uint32 window_flags = SDL_WINDOW_RESIZABLE;
#if !defined(XUO_EMSC)
    // on web the window is a plain event/size source: the WebGL2 context is
    // created and bound directly on the canvas by the renderer
    window_flags |= SDL_WINDOW_OPENGL;
#endif
    m_window = SDL_CreateWindow(title, 0, 0, width, height, window_flags);
    if (m_window == nullptr)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Coult not create window: %s\n", SDL_GetError());
        return false;
    }

    uint32_t rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    int shift = (g_icon.bytes_per_pixel == 3) ? 8 : 0;
    rmask = 0xff000000 >> shift;
    gmask = 0x00ff0000 >> shift;
    bmask = 0x0000ff00 >> shift;
    amask = 0x000000ff >> shift;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = (g_icon.bytes_per_pixel == 3) ? 0 : 0xff000000;
#endif

    SDL_Surface *icon = SDL_CreateRGBSurfaceFrom(
        (void *)g_icon.pixel_data,
        g_icon.width,
        g_icon.height,
        g_icon.bytes_per_pixel * 8,
        g_icon.bytes_per_pixel * g_icon.width,
        rmask,
        gmask,
        bmask,
        amask);
    SDL_SetWindowIcon(m_window, icon);
    SDL_FreeSurface(icon);

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(m_window, &info) != 0u)
    {
        Info(Client, "SDL %d.%d.%d", info.version.major, info.version.minor, info.version.patch);

        const char *subsystem = "Unknown";
        switch (info.subsystem)
        {
            case SDL_SYSWM_UNKNOWN:
                break;
#if SDL_VERSION_ATLEAST(2, 0, 5)
            case SDL_SYSWM_OS2:
                subsystem = "IBM OS/2";
                break;
#endif
            case SDL_SYSWM_WINDOWS:
                subsystem = "Microsoft Windows(TM)";
                break;
            case SDL_SYSWM_X11:
                subsystem = "X Window System";
                break;
#if SDL_VERSION_ATLEAST(2, 0, 3)
            case SDL_SYSWM_WINRT:
                subsystem = "WinRT";
                break;
#endif
            case SDL_SYSWM_DIRECTFB:
                subsystem = "DirectFB";
                break;
            case SDL_SYSWM_COCOA:
                subsystem = "Apple OS X";
                break;
            case SDL_SYSWM_UIKIT:
                subsystem = "UIKit";
                break;
#if SDL_VERSION_ATLEAST(2, 0, 2)
            case SDL_SYSWM_WAYLAND:
                subsystem = "Wayland";
                break;
            case SDL_SYSWM_MIR:
                subsystem = "Mir";
                break;
#endif
#if SDL_VERSION_ATLEAST(2, 0, 4)
            case SDL_SYSWM_ANDROID:
                subsystem = "Android";
                break;
#endif
#if SDL_VERSION_ATLEAST(2, 0, 5)
            case SDL_SYSWM_VIVANTE:
                subsystem = "Vivante";
                break;
#endif
            default:
                break;
        }

        Info(Client, "System: %s", subsystem);
#if defined(XUO_WINDOWS)
#endif
    }
    //SDL_SetWindowGrab(m_window, SDL_TRUE);
    SDL_RaiseWindow(m_window);
    SDL_ShowCursor(static_cast<int>(showCursor));

#if defined(XUO_EMSC)
    // keep filling the viewport after the game maximized (browser window
    // resize, devtools, device rotation); registered on the main browser
    // thread, the callback is delivered back here like SDL's own handlers
    emscripten_set_resize_callback(
        EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_FALSE, web_on_window_resize);
#endif

    return OnCreate();
}

void CGameWindow::Destroy()
{
    if (m_window != nullptr)
    {
        PushEvent(SDL_QUIT, nullptr, nullptr);
    }
}

void CGameWindow::ShowMessage(const astr_t &text, const astr_t &title)
{
    SDL_Log("%s: %s\n", title.c_str(), text.c_str());
}

void CGameWindow::ShowMessage(const wstr_t &text, const wstr_t &title)
{
    SDL_Log("%ls: %ls\n", title.c_str(), text.c_str());
}

bool CGameWindow::OnWindowProc(SDL_Event &ev)
{
    // only handle events belonging to this window; other windows in the
    // process (like the embedded launcher) have their own event handling.
    // Events without a window id (like SDL_QUIT) remain process-global.
    const auto id = SDL_GetWindowID(m_window);
    switch (ev.type)
    {
        case SDL_WINDOWEVENT:
            if (ev.window.windowID != id)
                return false;
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (ev.key.windowID != id)
                return false;
            break;
        case SDL_TEXTEDITING:
        case SDL_TEXTINPUT:
            if (ev.text.windowID != id)
                return false;
            break;
        case SDL_MOUSEMOTION:
            if (ev.motion.windowID != id)
                return false;
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            if (ev.button.windowID != id)
                return false;
            break;
        case SDL_MOUSEWHEEL:
            if (ev.wheel.windowID != id)
                return false;
            break;
        default:
            break;
    }

    switch (ev.type)
    {
        case SDL_WINDOWEVENT:
        {
            switch (ev.window.event)
            {
                case SDL_WINDOWEVENT_CLOSE:
                {
                    OnDestroy();
                    return true;
                }
                break;

                case SDL_WINDOWEVENT_SHOWN:
                {
                    OnActivate(); // Sound + FPS
                }
                break;

                case SDL_WINDOWEVENT_SIZE_CHANGED:
                case SDL_WINDOWEVENT_RESIZED:
                {
                    m_Size = { ev.window.data1, ev.window.data2 };
                    OnResize();
                }
                break;

                case SDL_WINDOWEVENT_MAXIMIZED:
                {
                    OnResize();
                }
                break;

                case SDL_WINDOWEVENT_RESTORED:
                {
                    int x, y, borderH = 0;
#if SDL_VERSION_ATLEAST(2, 0, 5)
                    SDL_GetWindowBordersSize(m_window, &borderH, nullptr, nullptr, nullptr);
#endif
                    SDL_GetWindowPosition(m_window, &x, &y);
                    SDL_SetWindowPosition(m_window, x, std::max(y, borderH));
                    OnResize();
                }
                break;

                case SDL_WINDOWEVENT_HIDDEN:
                {
                    OnDeactivate(); // Sound + FPS
                }
                break;

                default:
                    break;
            }
        }
        break;

        case SDL_QUIT:
        {
            OnDestroy();
            return true;
            break;
        }

        case SDL_KEYDOWN:
        {
            OnKeyDown(ev.key);
        }
        break;

        case SDL_KEYUP:
        {
            OnKeyUp(ev.key);
        }
        break;

        // https://wiki.libsdl.org/Tutorials/TextInput
        case SDL_TEXTINPUT: // WM_CHAR
        {
            OnTextInput(ev.text);
        }
        break;

        case SDL_MOUSEMOTION:
        {
            g_MouseManager.Update();
            if (g_MouseManager.Dragging)
            {
                OnDragging();
            }
        }
        break;

        case SDL_MOUSEWHEEL:
        {
            g_MouseManager.Update();

            const bool isUp = ev.wheel.y < 0;
            OnMidMouseButtonScroll(isUp);
        }
        break;

        case SDL_USEREVENT:
        {
            if (ev.user.code == SDL_QUIT)
            {
                OnDestroy();
                return true;
            }
            OnUserMessages(ev.user);
        }
        break;

        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEBUTTONDOWN:
        {
            g_MouseManager.Update();
            const bool isDown = ev.type == SDL_MOUSEBUTTONDOWN;
            auto &mouse = ev.button;
            switch (mouse.button)
            {
                case SDL_BUTTON_LEFT:
                    if (isDown)
                    {
                        g_MouseManager.Capture();
                        g_MouseManager.LeftButtonPressed = true;
                        g_MouseManager.LeftDropPosition = g_MouseManager.Position;
                        g_MouseManager.CancelDoubleClick = false;
                        uint32_t ticks = SDL_GetTicks();
                        if (g_MouseManager.LastLeftButtonClickTimer +
                                g_MouseManager.DoubleClickDelay >=
                            ticks)
                        {
                            g_MouseManager.LastLeftButtonClickTimer = 0;
                            if (!OnLeftMouseButtonDoubleClick())
                            {
                                OnLeftMouseButtonDown();
                            }
                            else
                            {
                                g_MouseManager.LastLeftButtonClickTimer = 0xFFFFFFFF;
                            }
                            break;
                        }

                        OnLeftMouseButtonDown();

                        if (g_MouseManager.CancelDoubleClick)
                        {
                            g_MouseManager.LastLeftButtonClickTimer = 0;
                        }
                        else
                        {
                            g_MouseManager.LastLeftButtonClickTimer = ticks;
                        }
                        break;
                    }
                    else
                    {
                        if (g_MouseManager.LastLeftButtonClickTimer != 0xFFFFFFFF)
                        {
                            OnLeftMouseButtonUp();
                        }
                        g_MouseManager.LeftButtonPressed = false;
                        g_MouseManager.Release();
                    }
                    break;

                case SDL_BUTTON_MIDDLE:
                    if (isDown)
                    {
                        g_MouseManager.Capture();
                        g_MouseManager.MidButtonPressed = true;
                        g_MouseManager.CancelDoubleClick = false;
                        uint32_t ticks = SDL_GetTicks();
                        if (g_MouseManager.LastMidButtonClickTimer +
                                g_MouseManager.DoubleClickDelay >=
                            ticks)
                        {
                            if (!OnMidMouseButtonDoubleClick())
                            {
                                OnMidMouseButtonDown();
                            }

                            g_MouseManager.LastMidButtonClickTimer = 0;
                            break;
                        }

                        OnMidMouseButtonDown();

                        if (g_MouseManager.CancelDoubleClick)
                        {
                            g_MouseManager.LastMidButtonClickTimer = 0;
                        }
                        else
                        {
                            g_MouseManager.LastMidButtonClickTimer = ticks;
                        }
                    }
                    else
                    {
                        OnMidMouseButtonUp();
                        g_MouseManager.MidButtonPressed = false;
                        g_MouseManager.Release();
                    }
                    break;

                case SDL_BUTTON_RIGHT:
                    if (isDown)
                    {
                        g_MouseManager.Capture();
                        g_MouseManager.RightButtonPressed = true;
                        g_MouseManager.CancelDoubleClick = false;
                        uint32_t ticks = SDL_GetTicks();
                        if (g_MouseManager.LastRightButtonClickTimer +
                                g_MouseManager.DoubleClickDelay >=
                            ticks)
                        {
                            g_MouseManager.LastRightButtonClickTimer = 0;
                            if (!OnRightMouseButtonDoubleClick())
                            {
                                OnRightMouseButtonDown();
                            }
                            else
                            {
                                g_MouseManager.LastRightButtonClickTimer = 0xFFFFFFFF;
                            }
                            break;
                        }

                        OnRightMouseButtonDown();

                        if (g_MouseManager.CancelDoubleClick)
                        {
                            g_MouseManager.LastRightButtonClickTimer = 0;
                        }
                        else
                        {
                            g_MouseManager.LastRightButtonClickTimer = ticks;
                        }
                    }
                    else
                    {
                        if (g_MouseManager.LastRightButtonClickTimer != 0xFFFFFFFF)
                        {
                            OnRightMouseButtonUp();
                        }
                        g_MouseManager.RightButtonPressed = false;
                        g_MouseManager.Release();
                    }
                    break;

                case SDL_BUTTON_X2:
                    break;

                default:
                    break;
            }
        }
        break;

        default:
            break;
    }

    return false;
}

bool CGameWindow::IsActive() const
{
    return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

void CGameWindow::SetTitle(const astr_t &text) const
{
    SDL_SetWindowTitle(m_window, text.c_str());
}

bool CGameWindow::IsMinimizedWindow() const
{
    return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED) != 0;
}

bool CGameWindow::IsMaximizedWindow() const
{
    return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MAXIMIZED) != 0;
}

static SDL_TimerID timer_table[FASTLOGIN_TIMER_ID] = {};

static uint32_t TimerCallbackEventPusher(uint32_t interval, void *param)
{
    PUSH_EVENT(CGameWindow::MessageID, param, nullptr);
    return interval;
}

void CGameWindow::CreateTimer(uint32_t id, int delay)
{
    assert(id > 0 && id < countof(timer_table));
    if (timer_table[id - 1] != 0)
    {
        return;
    }

    auto handle = SDL_AddTimer(delay, TimerCallbackEventPusher, (void *)(intptr_t)(id));
    timer_table[id - 1] = handle;
}

void CGameWindow::RemoveTimer(uint32_t id)
{
    assert(id < countof(timer_table));
    if (timer_table[id - 1] != 0)
    {
        SDL_RemoveTimer(timer_table[id - 1]);
    }
    timer_table[id - 1] = 0;
}

void CGameWindow::Raise()
{
    SDL_RaiseWindow(g_GameWindow.m_window);
}

uint32_t CGameWindow::PushEvent(uint32_t id, void *data1, void *data2)
{
    SDL_UserEvent userevent;
    userevent.type = SDL_USEREVENT;
    userevent.code = id;
    userevent.data1 = data1;
    userevent.data2 = data2;

    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_USEREVENT;
    event.user = userevent;
    SDL_PushEvent(&event);

    return 0;
}

void GetDisplaySize(int *x, int *y)
{
#if SDL_VERSION_ATLEAST(2, 0, 5)
    SDL_Rect r;
    SDL_GetDisplayUsableBounds(0, &r);
    if (x != nullptr)
    {
        *x = r.w;
    }
    if (y != nullptr)
    {
        *y = r.h;
    }
#else
    SDL_DisplayMode dm;
    SDL_GetCurrentDisplayMode(0, &dm);
    if (x != nullptr)
    {
        *x = dm.w;
    }
    if (y != nullptr)
    {
        *y = dm.h;
    }
#endif
}

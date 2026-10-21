// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Danny Angelo Carminati Grein

#include <algorithm>
#include "WispWindow.h"
#include "../Managers/MouseManager.h" // g_MouseManager
#include "../SDL_wrapper.h"
#include <common/utils.h> // countof
#include <external/gfx/gfx.h> // win_gfx_context_attrbutes
#include "icon_client.h"
#include "../GameWindow.h"

#if defined(XUO_EMSC)
#include <emscripten.h>
#include <emscripten/html5.h>
#endif


namespace Wisp
{
CWindow *g_WispWindow = nullptr;
WindowHandle CWindow::Handle = 0;

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
    if (s_webFillViewport && g_WispWindow != nullptr)
    {
        g_WispWindow->SetSize(web_viewport_size());
    }
    return EM_FALSE;
}
#endif

CWindow::CWindow()
{
    g_WispWindow = this;
}

CWindow::~CWindow()
{
}

void CWindow::SetMinSize(int width, int height)
{
    m_MinSize.Width = width;
    m_MinSize.Height = height;
}
void CWindow::SetMaxSize(int width, int height)
{
    m_MaxSize.Width = width;
    m_MaxSize.Height = height;
}

void CWindow::MaximizeWindow()
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

void CWindow::SetSize(const CSize &size)
{
    SDL_SetWindowSize(m_window, size.Width, size.Height);
    m_Size = size;
}

void CWindow::SetPositionSize(int x, int y, int width, int height)
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

void CWindow::GetPositionSize(int *x, int *y, int *width, int *height)
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

void CWindow::SetMinSize(const CSize &newMinSize)
{
    if (m_Size.Width < newMinSize.Width || m_Size.Height < newMinSize.Height)
    {
        int width = m_Size.Width;
        int height = m_Size.Height;

        if (width < newMinSize.Width)
        {
            width = newMinSize.Width;
        }

        if (height < newMinSize.Height)
        {
            height = newMinSize.Height;
        }

#if SDL_VERSION_ATLEAST(2, 0, 5)
        int borderH;
        SDL_GetWindowBordersSize(m_window, &borderH, nullptr, nullptr, nullptr);
#else
        int borderH = 0;
#endif
        SDL_SetWindowSize(m_window, width, height - borderH);
    }
    m_MinSize = newMinSize;
}

void CWindow::SetMaxSize(const CSize &newMaxSize)
{
    if (m_Size.Width > newMaxSize.Width || m_Size.Height > newMaxSize.Height)
    {
        int width = m_Size.Width;
        int height = m_Size.Height;

        if (width > newMaxSize.Width)
        {
            width = newMaxSize.Width;
        }

        if (height > newMaxSize.Height)
        {
            height = newMaxSize.Height;
        }
#if SDL_VERSION_ATLEAST(2, 0, 5)
        int borderH;
        SDL_GetWindowBordersSize(m_window, &borderH, nullptr, nullptr, nullptr);
#else
        int borderH = 0;
#endif
        SDL_SetWindowMaximumSize(m_window, newMaxSize.Width, newMaxSize.Height - borderH);
    }

    m_MaxSize = newMaxSize;
}

bool CWindow::Create(const char *title, bool showCursor, int width, int height)
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
        Handle = info.info.win.window;
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

void CWindow::Destroy()
{
    if (m_window != nullptr)
    {
        PushEvent(SDL_QUIT, nullptr, nullptr);
    }
}

void CWindow::ShowMessage(const astr_t &text, const astr_t &title)
{
    SDL_Log("%s: %s\n", title.c_str(), text.c_str());
}

void CWindow::ShowMessage(const wstr_t &text, const wstr_t &title)
{
    SDL_Log("%ls: %ls\n", title.c_str(), text.c_str());
}

bool CWindow::OnWindowProc(SDL_Event &ev)
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

bool CWindow::IsActive() const
{
    return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

void CWindow::SetTitle(const astr_t &text) const
{
    SDL_SetWindowTitle(m_window, text.c_str());
}

void CWindow::ShowWindow(bool show) const
{
    show ? SDL_ShowWindow(m_window) : SDL_HideWindow(m_window);
}

bool CWindow::IsMinimizedWindow() const
{
    return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED) != 0;
}

bool CWindow::IsMaximizedWindow() const
{
    return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MAXIMIZED) != 0;
}

static SDL_TimerID timer_table[FASTLOGIN_TIMER_ID] = {};

static uint32_t TimerCallbackEventPusher(uint32_t interval, void *param)
{
    PUSH_EVENT(CGameWindow::MessageID, param, nullptr);
    return interval;
}

void CWindow::CreateTimer(uint32_t id, int delay)
{
    assert(id > 0 && id < countof(timer_table));
    if (timer_table[id - 1] != 0)
    {
        return;
    }

    auto handle = SDL_AddTimer(delay, TimerCallbackEventPusher, (void *)(intptr_t)(id));
    timer_table[id - 1] = handle;
}

void CWindow::RemoveTimer(uint32_t id)
{
    assert(id < countof(timer_table));
    if (timer_table[id - 1] != 0)
    {
        SDL_RemoveTimer(timer_table[id - 1]);
    }
    timer_table[id - 1] = 0;
}

void CWindow::Raise()
{
    SDL_RaiseWindow(g_GameWindow.m_window);
}

uint32_t CWindow::PushEvent(uint32_t id, void *data1, void *data2)
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
}; // namespace Wisp

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

// MIT License

#include "WispWindow.h"
#include <SDL.h>
#include <SDL_syswm.h>
#include "../Icon.h"
#include "../Point.h"
#include "../GameWindow.h"
#include "../Managers/PluginManager.h"

namespace Wisp
{
CWindow *g_WispWindow = nullptr;
WindowHandle CWindow::Handle = 0;

CWindow::CWindow()
{
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;

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

bool CWindow::Create(
    const char *className, const char *title, bool showCursor, int width, int height)
{
    DEBUG_TRACE_FUNCTION;
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
    {
        return false;
    }

    m_Size.Width = width;
    m_Size.Height = height;
    m_window =
        SDL_CreateWindow(title, 0, 0, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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
        }

        Info(Client, "System: %s", subsystem);
#if defined(XUO_WINDOWS)
        Handle = info.info.win.window;
#endif
    }
    //SDL_SetWindowGrab(m_window, SDL_TRUE);
    SDL_RaiseWindow(m_window);
    SDL_ShowCursor(static_cast<int>(showCursor));

    return OnCreate();
}

void CWindow::Destroy()
{
    DEBUG_TRACE_FUNCTION;
    if (m_window != nullptr)
    {
        PushEvent(SDL_QUIT, nullptr, nullptr);
    }
}

void CWindow::ShowMessage(const string &text, const string &title)
{
    DEBUG_TRACE_FUNCTION;
    SDL_Log("%s: %s\n", title.c_str(), text.c_str());
}

void CWindow::ShowMessage(const wstring &text, const wstring &title)
{
    DEBUG_TRACE_FUNCTION;
    SDL_Log("%s: %s\n", title.c_str(), text.c_str());
}

bool CWindow::OnWindowProc(SDL_Event &ev)
{
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
                    OnShow(true); // Plugin
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
                    OnShow(false);  // Plugin
                    OnDeactivate(); // Sound + FPS
                }
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
            Wisp::g_WispMouse->Update();
            if (Wisp::g_WispMouse->Dragging)
            {
                OnDragging();
            }
        }
        break;

        case SDL_MOUSEWHEEL:
        {
            Wisp::g_WispMouse->Update();

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
            Wisp::g_WispMouse->Update();
            const bool isDown = ev.type == SDL_MOUSEBUTTONDOWN;
            auto &mouse = ev.button;
            switch (mouse.button)
            {
                case SDL_BUTTON_LEFT:
                    if (isDown)
                    {
                        Wisp::g_WispMouse->Capture();
                        Wisp::g_WispMouse->LeftButtonPressed = true;
                        Wisp::g_WispMouse->LeftDropPosition = Wisp::g_WispMouse->Position;
                        Wisp::g_WispMouse->CancelDoubleClick = false;
                        uint32_t ticks = SDL_GetTicks();
                        if (Wisp::g_WispMouse->LastLeftButtonClickTimer +
                                Wisp::g_WispMouse->DoubleClickDelay >=
                            ticks)
                        {
                            Wisp::g_WispMouse->LastLeftButtonClickTimer = 0;
                            if (!OnLeftMouseButtonDoubleClick())
                            {
                                OnLeftMouseButtonDown();
                            }
                            else
                            {
                                Wisp::g_WispMouse->LastLeftButtonClickTimer = 0xFFFFFFFF;
                            }
                            break;
                        }

                        OnLeftMouseButtonDown();

                        if (Wisp::g_WispMouse->CancelDoubleClick)
                        {
                            Wisp::g_WispMouse->LastLeftButtonClickTimer = 0;
                        }
                        else
                        {
                            Wisp::g_WispMouse->LastLeftButtonClickTimer = ticks;
                        }
                        break;
                    }
                    else
                    {
                        if (Wisp::g_WispMouse->LastLeftButtonClickTimer != 0xFFFFFFFF)
                        {
                            OnLeftMouseButtonUp();
                        }
                        Wisp::g_WispMouse->LeftButtonPressed = false;
                        Wisp::g_WispMouse->Release();
                    }
                    break;

                case SDL_BUTTON_MIDDLE:
                    if (isDown)
                    {
                        Wisp::g_WispMouse->Capture();
                        Wisp::g_WispMouse->MidButtonPressed = true;
                        Wisp::g_WispMouse->MidDropPosition = Wisp::g_WispMouse->Position;
                        Wisp::g_WispMouse->CancelDoubleClick = false;
                        uint32_t ticks = SDL_GetTicks();
                        if (Wisp::g_WispMouse->LastMidButtonClickTimer +
                                Wisp::g_WispMouse->DoubleClickDelay >=
                            ticks)
                        {
                            if (!OnMidMouseButtonDoubleClick())
                            {
                                OnMidMouseButtonDown();
                            }

                            Wisp::g_WispMouse->LastMidButtonClickTimer = 0;
                            break;
                        }

                        OnMidMouseButtonDown();

                        if (Wisp::g_WispMouse->CancelDoubleClick)
                        {
                            Wisp::g_WispMouse->LastMidButtonClickTimer = 0;
                        }
                        else
                        {
                            Wisp::g_WispMouse->LastMidButtonClickTimer = ticks;
                        }
                    }
                    else
                    {
                        OnMidMouseButtonUp();
                        Wisp::g_WispMouse->MidButtonPressed = false;
                        Wisp::g_WispMouse->Release();
                    }
                    break;

                case SDL_BUTTON_RIGHT:
                    if (isDown)
                    {
                        Wisp::g_WispMouse->Capture();
                        Wisp::g_WispMouse->RightButtonPressed = true;
                        Wisp::g_WispMouse->RightDropPosition = Wisp::g_WispMouse->Position;
                        Wisp::g_WispMouse->CancelDoubleClick = false;
                        uint32_t ticks = SDL_GetTicks();
                        if (Wisp::g_WispMouse->LastRightButtonClickTimer +
                                Wisp::g_WispMouse->DoubleClickDelay >=
                            ticks)
                        {
                            Wisp::g_WispMouse->LastRightButtonClickTimer = 0;
                            if (!OnRightMouseButtonDoubleClick())
                            {
                                OnRightMouseButtonDown();
                            }
                            else
                            {
                                Wisp::g_WispMouse->LastRightButtonClickTimer = 0xFFFFFFFF;
                            }
                            break;
                        }

                        OnRightMouseButtonDown();

                        if (Wisp::g_WispMouse->CancelDoubleClick)
                        {
                            Wisp::g_WispMouse->LastRightButtonClickTimer = 0;
                        }
                        else
                        {
                            Wisp::g_WispMouse->LastRightButtonClickTimer = ticks;
                        }
                    }
                    else
                    {
                        if (Wisp::g_WispMouse->LastRightButtonClickTimer != 0xFFFFFFFF)
                        {
                            OnRightMouseButtonUp();
                        }
                        Wisp::g_WispMouse->RightButtonPressed = false;
                        Wisp::g_WispMouse->Release();
                    }
                    break;

                case SDL_BUTTON_X1:
                    OnXMouseButton(!isDown);
                    break;

                case SDL_BUTTON_X2:
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

void CWindow::SetTitle(const string &text) const
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
    assert(id >= 0 && id < countof(timer_table));
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

uint32_t CWindow::PluginEvent(uint32_t id, const void *data)
{
    return g_PluginManager.OnEvent(id, data);
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

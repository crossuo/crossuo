// MIT License

#include "stdafx.h"
#include "WispWindow.h"
#include <SDL.h>
#include <SDL_syswm.h>
#include <SDL_timer.h>
namespace Wisp
{
CWindow *g_WispWindow = nullptr;

#if USE_WISP
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    DEBUG_TRACE_FUNCTION;
    if (g_WispWindow != nullptr)
        return g_WispWindow->OnWindowProc(hWnd, message, wParam, lParam);

    return DefWindowProc(hWnd, message, wParam, lParam);
}
#endif

CWindow::CWindow()
{
    DEBUG_TRACE_FUNCTION;
    g_WispWindow = this;
}

CWindow::~CWindow()
{
}

void CWindow::SetSize(const Wisp::CSize &size)
{
#if USE_WISP
    DEBUG_TRACE_FUNCTION;
    RECT pos = { 0, 0, 0, 0 };
    GetWindowRect(Handle, &pos);

    RECT r = { 0, 0, 0, 0 };
    r.right = size.Width;
    r.bottom = size.Height;
    AdjustWindowRectEx(
        &r, GetWindowLongA(Handle, GWL_STYLE), FALSE, GetWindowLongA(Handle, GWL_EXSTYLE));

    if (r.left < 0)
        r.right += -r.left;

    if (r.top < 0)
        r.bottom += -r.top;

    SetWindowPos(Handle, HWND_TOP, pos.left, pos.top, r.right, r.bottom, 0);
#else
    SDL_SetWindowSize(m_window, size.Width, size.Height);
#endif
    m_Size = size;
}

void CWindow::SetPositionSize(int x, int y, int width, int height)
{
#if USE_WISP
    SetWindowPos(Handle, nullptr, x, y, width, height, 0);
#else
    SDL_SetWindowPosition(m_window, x, y);
    SDL_SetWindowSize(m_window, width, height);
#endif
}

void CWindow::GetPositionSize(int *x, int *y, int *width, int *height)
{
#if USE_WISP
    RECT rect{};
    GetWindowRect(Handle, &rect);
    *x = (int)rect.left;
    *y = (int)rect.top;
    *width = (int)(rect.right - rect.left);
    *height = (int)(rect.bottom - rect.top);
#else
    SDL_GetWindowPosition(m_window, x, y);
    SDL_GetWindowSize(m_window, width, height);
#endif
}

void CWindow::SetMinSize(const Wisp::CSize &newMinSize)
{
    DEBUG_TRACE_FUNCTION;
    if (m_Size.Width < newMinSize.Width || m_Size.Height < newMinSize.Height)
    {
        int width = m_Size.Width;
        int height = m_Size.Height;

        if (width < newMinSize.Width)
            width = newMinSize.Width;

        if (height < newMinSize.Height)
            height = newMinSize.Height;

#if USE_WISP
        RECT pos = { 0, 0, 0, 0 };
        GetWindowRect(Handle, &pos);

        RECT r = { 0, 0, 0, 0 };
        r.right = width;
        r.bottom = height;
        AdjustWindowRectEx(
            &r, GetWindowLongA(Handle, GWL_STYLE), FALSE, GetWindowLongA(Handle, GWL_EXSTYLE));

        if (r.left < 0)
            r.right += -r.left;

        if (r.top < 0)
            r.bottom += -r.top;

        SetWindowPos(Handle, HWND_TOP, pos.left, pos.top, r.right, r.bottom, 0);
#else
        SDL_SetWindowSize(m_window, width, height);
#endif
    }
    m_MinSize = newMinSize;
}

void CWindow::SetMaxSize(const Wisp::CSize &newMaxSize)
{
    DEBUG_TRACE_FUNCTION;

    if (m_Size.Width > newMaxSize.Width || m_Size.Height > newMaxSize.Height)
    {
        int width = m_Size.Width;
        int height = m_Size.Height;

        if (width > newMaxSize.Width)
            width = newMaxSize.Width;

        if (height > newMaxSize.Height)
            height = newMaxSize.Height;

#if USE_WISP
        RECT pos = { 0, 0, 0, 0 };
        GetWindowRect(Handle, &pos);

        RECT r = { 0, 0, 0, 0 };
        r.right = width;
        r.bottom = height;
        AdjustWindowRectEx(
            &r, GetWindowLongA(Handle, GWL_STYLE), FALSE, GetWindowLongA(Handle, GWL_EXSTYLE));

        if (r.left < 0)
            r.right += -r.left;

        if (r.top < 0)
            r.bottom += -r.top;

        SetWindowPos(Handle, HWND_TOP, pos.left, pos.top, r.right, r.bottom, 0);
#else
        SDL_SetWindowMaximumSize(m_window, newMaxSize.Width, newMaxSize.Height);
#endif
    }

    m_MaxSize = newMaxSize;
}

bool CWindow::Create(
    const char *className, const char *title, bool showCursor, int width, int height)
{
    DEBUG_TRACE_FUNCTION;

#if USE_WISP
    HICON icon = LoadIcon(g_OrionWindow.hInstance, MAKEINTRESOURCE(IDI_ORIONUO));
    HCURSOR cursor = LoadCursor(g_OrionWindow.hInstance, MAKEINTRESOURCE(IDC_CURSOR1));

    static wstring wclassName = ToWString(className);
    static wstring wtitle = ToWString(title);
    WNDCLASSEX wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wcex.lpfnWndProc = WindowProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hCursor = cursor;
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = wclassName.c_str();
    wcex.hIcon = icon;
    wcex.hIconSm = icon;

    RegisterClassEx(&wcex);

    width += 2 * GetSystemMetrics(SM_CXSIZEFRAME);
    height += GetSystemMetrics(SM_CYCAPTION) + (GetSystemMetrics(SM_CYFRAME) * 2);

    Handle = CreateWindowEx(
        WS_EX_WINDOWEDGE,
        wclassName.c_str(),
        wtitle.c_str(),
        WS_OVERLAPPEDWINDOW,
        0,
        0,
        width,
        height,
        nullptr,
        nullptr,
        hInstance,
        nullptr);
    if (!Handle)
        return false;

    RECT r = { 0, 0, 0, 0 };
    r.right = width;
    r.bottom = height;
    AdjustWindowRectEx(
        &r, GetWindowLongA(Handle, GWL_STYLE), FALSE, GetWindowLongA(Handle, GWL_EXSTYLE));

    if (r.left < 0)
        r.right += -r.left;

    if (r.top < 0)
        r.bottom += -r.top;

    SetWindowPos(Handle, HWND_TOP, 0, 0, r.right, r.bottom, 0);

    srand(unsigned(time(nullptr)));

    GetClientRect(Handle, &r);
    m_Size.Width = r.right - r.left;
    m_Size.Height = r.bottom - r.top;

    ::ShowCursor(showCursor);
    ::ShowWindow(Handle, FALSE);
    ::UpdateWindow(Handle);
#else
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
        return false;

    m_Size.Width = width;
    m_Size.Height = height;
    m_window = SDL_CreateWindow(
        title,
        0,
        0,
        width,
        height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!m_window)
    {
        SDL_LogError(SDL_LOG_CATEGORY_VIDEO, "Coult not create window: %s\n", SDL_GetError());
        return false;
    }

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(m_window, &info))
    {
        SDL_Log("SDL %d.%d.%d\n", info.version.major, info.version.minor, info.version.patch);

        const char *subsystem = "Unknown";
        switch (info.subsystem)
        {
            case SDL_SYSWM_UNKNOWN:
                break;
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

        SDL_Log("System: %s\n", subsystem);
#if defined(ORION_WINDOWS)
        Handle = info.info.win.window;
#endif
    }

    SDL_ShowCursor(showCursor);
#endif // USE_WISP

    return OnCreate();
}

void CWindow::Destroy()
{
    DEBUG_TRACE_FUNCTION;
#if USE_WISP
    PostMessage(Handle, WM_CLOSE, 0, 0);
#else
    if (m_window)
        SDL_DestroyWindow(m_window);
#endif
}

void CWindow::ShowMessage(const string &text, const string &title)
{
    DEBUG_TRACE_FUNCTION;
#if USE_WISP
    MessageBoxA(Handle, text.c_str(), title.c_str(), MB_OK);
#else
    SDL_Log("%s: %s\n", title.c_str(), text.c_str());
#endif
}

void CWindow::ShowMessage(const wstring &text, const wstring &title)
{
    DEBUG_TRACE_FUNCTION;
#if USE_WISP
    MessageBoxW(Handle, text.c_str(), title.c_str(), MB_OK);
#else
    SDL_Log("%s: %s\n", title.c_str(), text.c_str());
#endif
}

#if USE_WISP
LRESULT CWindow::OnWindowProc(HWND &hWnd, UINT &message, WPARAM &wParam, LPARAM &lParam)
{
    DEBUG_TRACE_FUNCTION;
    //DebugMsg("m=0x%08X, w0x%08X l0x%08X\n", message, wParam, lParam);
    static bool parse = true;

    if (!parse)
        return DefWindowProc(hWnd, message, wParam, lParam);

    switch (message)
    {
        case WM_SETCURSOR:
        {
            if (LOWORD(lParam) == HTCLIENT)
            {
                SetCursor(nullptr);
                return 0;
            }
            break;
        }
        case WM_GETMINMAXINFO:
        case WM_SIZE:
        {
            if (IsMinimizedWindow())
                return DefWindowProc(hWnd, message, wParam, lParam);

            if (message == WM_GETMINMAXINFO)
            {
                MINMAXINFO *pInfo = (MINMAXINFO *)lParam;

                if (NoResize)
                {
                    RECT r = { 0, 0, 0, 0 };
                    r.right = m_Size.Width;
                    r.bottom = m_Size.Height;
                    AdjustWindowRectEx(
                        &r,
                        GetWindowLongA(Handle, GWL_STYLE),
                        FALSE,
                        GetWindowLongA(Handle, GWL_EXSTYLE));

                    if (r.left < 0)
                        r.right -= r.left;

                    if (r.top < 0)
                        r.bottom -= r.top;

                    POINT min = { r.right, r.bottom };
                    POINT max = { r.right, r.bottom };

                    pInfo->ptMinTrackSize = min;
                    pInfo->ptMaxTrackSize = max;
                }
                else
                {
                    RECT r = { 0, 0, 0, 0 };
                    r.right = m_Size.Width;
                    r.bottom = m_Size.Height;
                    AdjustWindowRectEx(
                        &r,
                        GetWindowLongA(Handle, GWL_STYLE),
                        FALSE,
                        GetWindowLongA(Handle, GWL_EXSTYLE));

                    if (r.left < 0)
                        r.right -= r.left;

                    if (r.top < 0)
                        r.bottom -= r.top;

                    POINT min = { m_MinSize.Width, m_MinSize.Height };
                    POINT max = { m_MaxSize.Width, m_MaxSize.Height };
                    pInfo->ptMinTrackSize = min;
                    pInfo->ptMaxTrackSize = max;
                }

                return 0;
            }

            Wisp::CSize newSize(LOWORD(lParam), HIWORD(lParam));

            OnResize(newSize);
            m_Size = newSize;

            break;
        }
        case WM_CLOSE:
        case WM_NCDESTROY:
        case WM_DESTROY:
        {
            parse = false;

            OnDestroy();

            //ExitProcess(0);
            PostQuitMessage(0);

            return 0;
        }
        case WM_MOUSEMOVE:
        {
            Wisp::g_WispMouse->LeftButtonPressed = (bool)(wParam & MK_LBUTTON);
            Wisp::g_WispMouse->RightButtonPressed = (bool)(wParam & MK_RBUTTON);
            Wisp::g_WispMouse->MidButtonPressed = (bool)(wParam & MK_MBUTTON);
            Wisp::g_WispMouse->Update(); // TODO: check if is correct

            if (Wisp::g_WispMouse->Dragging)
                OnDragging();

            break;
        }
        case WM_LBUTTONDOWN:
        {
            Wisp::g_WispMouse->Capture();

            Wisp::g_WispMouse->Update();
            Wisp::g_WispMouse->LeftButtonPressed = true;
            Wisp::g_WispMouse->LeftDropPosition = Wisp::g_WispMouse->Position;
            Wisp::g_WispMouse->CancelDoubleClick = false;

            uint ticks = SDL_GetTicks();

            if (Wisp::g_WispMouse->LastLeftButtonClickTimer + Wisp::g_WispMouse->DoubleClickDelay >=
                ticks)
            {
                Wisp::g_WispMouse->LastLeftButtonClickTimer = 0;

                if (!OnLeftMouseButtonDoubleClick())
                    OnLeftMouseButtonDown();
                else
                    Wisp::g_WispMouse->LastLeftButtonClickTimer = 0xFFFFFFFF;

                break;
            }

            OnLeftMouseButtonDown();

            if (Wisp::g_WispMouse->CancelDoubleClick)
                Wisp::g_WispMouse->LastLeftButtonClickTimer = 0;
            else
                Wisp::g_WispMouse->LastLeftButtonClickTimer = ticks;

            break;
        }
        case WM_LBUTTONUP:
        {
            Wisp::g_WispMouse->Update();

            if (Wisp::g_WispMouse->LastLeftButtonClickTimer != 0xFFFFFFFF)
                OnLeftMouseButtonUp();

            Wisp::g_WispMouse->LeftButtonPressed = false;
            Wisp::g_WispMouse->Release();

            break;
        }
        case WM_RBUTTONDOWN:
        {
            Wisp::g_WispMouse->Capture();

            Wisp::g_WispMouse->Update();
            Wisp::g_WispMouse->RightButtonPressed = true;
            Wisp::g_WispMouse->RightDropPosition = Wisp::g_WispMouse->Position;
            Wisp::g_WispMouse->CancelDoubleClick = false;

            uint ticks = SDL_GetTicks();

            if (Wisp::g_WispMouse->LastRightButtonClickTimer +
                    Wisp::g_WispMouse->DoubleClickDelay >=
                ticks)
            {
                Wisp::g_WispMouse->LastRightButtonClickTimer = 0;

                if (!OnRightMouseButtonDoubleClick())
                    OnRightMouseButtonDown();
                else
                    Wisp::g_WispMouse->LastRightButtonClickTimer = 0xFFFFFFFF;

                break;
            }

            OnRightMouseButtonDown();

            if (Wisp::g_WispMouse->CancelDoubleClick)
                Wisp::g_WispMouse->LastRightButtonClickTimer = 0;
            else
                Wisp::g_WispMouse->LastRightButtonClickTimer = ticks;

            break;
        }
        case WM_RBUTTONUP:
        {
            Wisp::g_WispMouse->Update();

            if (Wisp::g_WispMouse->LastRightButtonClickTimer != 0xFFFFFFFF)
                OnRightMouseButtonUp();

            Wisp::g_WispMouse->RightButtonPressed = false;
            Wisp::g_WispMouse->Release();

            break;
        }
        //Нажатие на колесико мышки
        case WM_MBUTTONDOWN:
        {
            Wisp::g_WispMouse->Capture();

            Wisp::g_WispMouse->Update();
            Wisp::g_WispMouse->MidButtonPressed = true;
            Wisp::g_WispMouse->MidDropPosition = Wisp::g_WispMouse->Position;
            Wisp::g_WispMouse->CancelDoubleClick = false;

            uint ticks = SDL_GetTicks();

            if (Wisp::g_WispMouse->LastMidButtonClickTimer + Wisp::g_WispMouse->DoubleClickDelay >=
                ticks)
            {
                if (!OnMidMouseButtonDoubleClick())
                    OnMidMouseButtonDown();

                Wisp::g_WispMouse->LastMidButtonClickTimer = 0;

                break;
            }

            OnMidMouseButtonDown();

            if (Wisp::g_WispMouse->CancelDoubleClick)
                Wisp::g_WispMouse->LastMidButtonClickTimer = 0;
            else
                Wisp::g_WispMouse->LastMidButtonClickTimer = ticks;

            break;
        }
        //Отпускание колесика мышки
        case WM_MBUTTONUP:
        {
            Wisp::g_WispMouse->Update();
            OnMidMouseButtonUp();

            Wisp::g_WispMouse->MidButtonPressed = false;
            Wisp::g_WispMouse->Release();

            break;
        }
        //Колесико мышки вверх/вниз
        case WM_MOUSEWHEEL:
        {
            Wisp::g_WispMouse->Update();
            OnMidMouseButtonScroll(!(short(HIWORD(wParam)) > 0));

            break;
        }
        //Доп. кнопки мыши
        case WM_XBUTTONDOWN:
        {
            Wisp::g_WispMouse->Update();
            OnXMouseButton(!(short(HIWORD(wParam)) > 0));

            break;
        }
        case WM_CHAR:
        {
            OnCharPress(wParam, lParam);

            return 0; //break;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        {
            OnKeyDown({ wParam, lParam });

            if (wParam == KEY_F4 && (GetAsyncKeyState(KEY_MENU) & 0x80000000)) //Alt + F4
                break;

            return 0; //break;
        }
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            OnKeyUp({ wParam, lParam });

            return 0; //break;
        }
        case WM_NCACTIVATE:
        {
            HRESULT res = (HRESULT)DefWindowProc(Handle, WM_NCACTIVATE, wParam, lParam);

            if (wParam == 0)
                OnDeactivate();
            else
                OnActivate();

            return res;
        }
        case WM_NCPAINT:
            return OnRepaint(wParam, lParam);
        case WM_SHOWWINDOW:
        {
            HRESULT res = (HRESULT)DefWindowProc(Handle, WM_SHOWWINDOW, wParam, lParam);

            OnShow(wParam != 0);

            return res;
        }
        case WM_SETTEXT:
        {
            HRESULT res = (HRESULT)DefWindowProc(Handle, WM_SETTEXT, wParam, lParam);

            OnSetText(lParam);

            return res;
        }
        case WM_TIMER:
        {
            OnTimer((uint)wParam);

            break;
        }
        case Wisp::CThreadedTimer::MessageID:
        {
            OnThreadedTimer((uint)wParam, (Wisp::CThreadedTimer *)lParam);

            //DebugMsg("OnThreadedTimer %i, 0x%08X\n", wParam, lParam);

            return 0;
        }
        case WM_SYSCHAR:
        {
            if (wParam == KEY_F4 && (GetAsyncKeyState(KEY_MENU) & 0x80000000)) //Alt + F4
                break;
            return 0;
        }
        default:
            break;
    }

    if (message >= WM_USER)
        return OnUserMessages(message, wParam, lParam);

    return DefWindowProc(hWnd, message, wParam, lParam);
}
#else
bool CWindow::OnWindowProc(SDL_Event &ev)
{
    switch (ev.type)
    {
        case SDL_WINDOWEVENT_CLOSE:
        case SDL_QUIT:
        {
            OnDestroy();
            return true;
            break;
        }

        case SDL_WINDOWEVENT_SHOWN:
        {
            OnShow(true); // Plugin
            OnActivate(); // Sound + FPS
        }
        break;

        case SDL_WINDOWEVENT_HIDDEN:
        {
            OnShow(false);  // Plugin
            OnDeactivate(); // Sound + FPS
        }
        break;

            // FIXME
            //case WM_GETMINMAXINFO:
            //case WM_SIZE:
            // Wisp::CThreadedTimer::MessageID:
            //case WM_TIMER:

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
                OnDragging();
        }
        break;

        case SDL_MOUSEWHEEL:
        {
            Wisp::g_WispMouse->Update();

            const bool isUp = ev.wheel.y > 0;
            OnMidMouseButtonScroll(isUp);
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
                    Wisp::g_WispMouse->LeftButtonPressed = isDown;
                    if (isDown)
                    {
                        Wisp::g_WispMouse->Capture();
                        Wisp::g_WispMouse->LeftDropPosition = Wisp::g_WispMouse->Position;
                        Wisp::g_WispMouse->CancelDoubleClick = false;
                        uint ticks = SDL_GetTicks();
                        if (Wisp::g_WispMouse->LastLeftButtonClickTimer +
                                Wisp::g_WispMouse->DoubleClickDelay >=
                            ticks)
                        {
                            Wisp::g_WispMouse->LastLeftButtonClickTimer = 0;
                            if (!OnLeftMouseButtonDoubleClick())
                                OnLeftMouseButtonDown();
                            else
                                Wisp::g_WispMouse->LastLeftButtonClickTimer = 0xFFFFFFFF;
                            break;
                        }

                        OnLeftMouseButtonDown();

                        if (Wisp::g_WispMouse->CancelDoubleClick)
                            Wisp::g_WispMouse->LastLeftButtonClickTimer = 0;
                        else
                            Wisp::g_WispMouse->LastLeftButtonClickTimer = ticks;
                        break;
                    }
                    else
                    {
                        if (Wisp::g_WispMouse->LastLeftButtonClickTimer != 0xFFFFFFFF)
                            OnLeftMouseButtonUp();
                        Wisp::g_WispMouse->Release();
                    }
                    break;

                case SDL_BUTTON_MIDDLE:
                    Wisp::g_WispMouse->MidButtonPressed = isDown;
                    if (isDown)
                    {
                        Wisp::g_WispMouse->Capture();
                        Wisp::g_WispMouse->MidDropPosition = Wisp::g_WispMouse->Position;
                        Wisp::g_WispMouse->CancelDoubleClick = false;
                        uint ticks = SDL_GetTicks();
                        if (Wisp::g_WispMouse->LastMidButtonClickTimer +
                                Wisp::g_WispMouse->DoubleClickDelay >=
                            ticks)
                        {
                            if (!OnMidMouseButtonDoubleClick())
                                OnMidMouseButtonDown();

                            Wisp::g_WispMouse->LastMidButtonClickTimer = 0;
                            break;
                        }

                        OnMidMouseButtonDown();

                        if (Wisp::g_WispMouse->CancelDoubleClick)
                            Wisp::g_WispMouse->LastMidButtonClickTimer = 0;
                        else
                            Wisp::g_WispMouse->LastMidButtonClickTimer = ticks;
                    }
                    else
                    {
                        OnMidMouseButtonUp();
                        Wisp::g_WispMouse->Release();
                    }
                    break;

                case SDL_BUTTON_RIGHT:
                    Wisp::g_WispMouse->RightButtonPressed = isDown;
                    if (isDown)
                    {
                        Wisp::g_WispMouse->Capture();
                        Wisp::g_WispMouse->RightDropPosition = Wisp::g_WispMouse->Position;
                        Wisp::g_WispMouse->CancelDoubleClick = false;
                        uint ticks = SDL_GetTicks();
                        if (Wisp::g_WispMouse->LastRightButtonClickTimer +
                                Wisp::g_WispMouse->DoubleClickDelay >=
                            ticks)
                        {
                            Wisp::g_WispMouse->LastRightButtonClickTimer = 0;
                            if (!OnRightMouseButtonDoubleClick())
                                OnRightMouseButtonDown();
                            else
                                Wisp::g_WispMouse->LastRightButtonClickTimer = 0xFFFFFFFF;
                            break;
                        }

                        OnRightMouseButtonDown();

                        if (Wisp::g_WispMouse->CancelDoubleClick)
                            Wisp::g_WispMouse->LastRightButtonClickTimer = 0;
                        else
                            Wisp::g_WispMouse->LastRightButtonClickTimer = ticks;
                    }
                    else
                    {
                        if (Wisp::g_WispMouse->LastRightButtonClickTimer != 0xFFFFFFFF)
                            OnRightMouseButtonUp();
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

            // Used by plugins only?
            // - OnShow
            // - OnRepaint
            // - OnSetText
            // - OnXMouseButton

        default:
            break;
    }

    //g_Orion.Process(true);
    //g_Orion.Process(false);

    return false;
}
#endif

void CWindow::CreateThreadedTimer(
    uint id, int delay, bool oneShot, bool waitForProcessMessage, bool synchronizedDelay)
{
    DEBUG_TRACE_FUNCTION;
    for (deque<Wisp::CThreadedTimer *>::iterator i = m_ThreadedTimersStack.begin();
         i != m_ThreadedTimersStack.end();
         ++i)
    {
        if ((*i)->TimerID == id)
            return;
    }

    Wisp::CThreadedTimer *timer = new Wisp::CThreadedTimer(id, Handle, waitForProcessMessage);
    m_ThreadedTimersStack.push_back(timer);
    timer->Run(!oneShot, delay, synchronizedDelay);
}

void CWindow::RemoveThreadedTimer(uint id)
{
    DEBUG_TRACE_FUNCTION;
    for (deque<Wisp::CThreadedTimer *>::iterator i = m_ThreadedTimersStack.begin();
         i != m_ThreadedTimersStack.end();
         ++i)
    {
        if ((*i)->TimerID == id)
        {
            (*i)->Stop();
            m_ThreadedTimersStack.erase(i);

            break;
        }
    }
}

Wisp::CThreadedTimer *CWindow::GetThreadedTimer(uint id)
{
    DEBUG_TRACE_FUNCTION;
    for (deque<Wisp::CThreadedTimer *>::iterator i = m_ThreadedTimersStack.begin();
         i != m_ThreadedTimersStack.end();
         ++i)
    {
        if ((*i)->TimerID == id)
            return *i;
    }

    return 0;
}
}; // namespace Wisp

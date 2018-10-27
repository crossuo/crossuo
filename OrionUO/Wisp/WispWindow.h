// MIT License

#pragma once
#include <SDL_video.h>
#include "Input.h"

namespace Wisp
{
class CWindow
{
public:
    HWND Handle = 0;
    bool NoResize = false;

protected:
    Wisp::CSize m_Size = Wisp::CSize();
    Wisp::CSize m_MinSize = Wisp::CSize(100, 100);
    Wisp::CSize m_MaxSize =
        Wisp::CSize(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));

public:
    Wisp::CSize GetSize() { return m_Size; };
    void SetSize(const Wisp::CSize &val);

    Wisp::CSize GetMinSize() { return m_MinSize; };
    void SetMinSize(const Wisp::CSize &val);

    Wisp::CSize GetMaxSize() { return m_MaxSize; };
    void SetMaxSize(const Wisp::CSize &val);

    void GetPositionSize(int *x, int *y, int *width, int *height);
    void SetPositionSize(int x, int y, int width, int height);

    // GetSystemMetrics -> SDL_GetCurrentDisplayMode
    SDL_Window *m_window = nullptr;

private:
    deque<Wisp::CThreadedTimer *> m_ThreadedTimersStack;

public:
    CWindow();
    virtual ~CWindow();

    void SetMinSize(int width, int height)
    {
        m_MinSize.Width = width;
        m_MinSize.Height = height;
    }
    void SetMaxSize(int width, int height)
    {
        m_MaxSize.Width = width;
        m_MaxSize.Height = height;
    }

    bool Create(
        const char *className,
        const char *title,
        bool showCursor = false,
        int width = 800,
        int height = 600);
    void Destroy();

    void ShowMessage(const string &text, const string &title);
    void ShowMessage(const wstring &text, const wstring &title);

#if USE_WISP
    HINSTANCE hInstance = 0;
    LRESULT OnWindowProc(HWND &hWnd, UINT &message, WPARAM &wParam, LPARAM &lParam);
#else
    bool OnWindowProc(SDL_Event &ev);
#endif

#if USE_WISP
    bool IsActive() const { return (::GetForegroundWindow() == Handle); }
    void SetTitle(const string &text) const { ::SetWindowTextA(Handle, text.c_str()); }
    void ShowWindow(bool show) const { ::ShowWindow(Handle, show ? TRUE : FALSE); }
    bool IsMinimizedWindow() const { return ::IsIconic(Handle); }
    bool IsMaximizedWindow() const { return (::IsZoomed(Handle) != FALSE); }
#else
    bool IsActive() const { return SDL_GetGrabbedWindow() == m_window; } // TODO: check
    void SetTitle(const string &text) const { SDL_SetWindowTitle(m_window, text.c_str()); }
    void ShowWindow(bool show) const { show ? SDL_ShowWindow(m_window) : SDL_HideWindow(m_window); }
    bool IsMinimizedWindow() const
    {
        return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MINIMIZED) != 0;
    }
    bool IsMaximizedWindow() const
    {
        return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MAXIMIZED) != 0;
    }
#endif

    // May be done using: SDL_AddTimer / SDL_RemoveTimer
    void CreateTimer(uint id, int delay) { ::SetTimer(Handle, id, delay, nullptr); }
    void RemoveTimer(uint id) { ::KillTimer(Handle, id); }

    void CreateThreadedTimer(
        uint id,
        int delay,
        bool oneShot = false,
        bool waitForProcessMessage = true,
        bool synchronizedDelay = false);
    void RemoveThreadedTimer(uint id);
    Wisp::CThreadedTimer *GetThreadedTimer(uint id);

protected:
    virtual bool OnCreate() { return true; }
    virtual void OnDestroy() {}
    virtual void OnResize(Wisp::CSize &newSize) {}
    virtual void OnLeftMouseButtonDown() {}
    virtual void OnLeftMouseButtonUp() {}
    virtual bool OnLeftMouseButtonDoubleClick() { return false; }
    virtual void OnRightMouseButtonDown() {}
    virtual void OnRightMouseButtonUp() {}
    virtual bool OnRightMouseButtonDoubleClick() { return false; }
    virtual void OnMidMouseButtonDown() {}
    virtual void OnMidMouseButtonUp() {}
    virtual bool OnMidMouseButtonDoubleClick() { return false; }
    virtual void OnMidMouseButtonScroll(bool up) {}
    virtual void OnXMouseButton(bool up) {}
    virtual void OnDragging() {}
    virtual void OnActivate() {}
    virtual void OnDeactivate() {}
    virtual void OnShow(bool show) {}

    virtual void OnTimer(uint id) {}
    virtual void OnThreadedTimer(uint nowTime, Wisp::CThreadedTimer *timer) {}
    virtual void OnSetText(const LPARAM &lParam) {}
    virtual HRESULT OnRepaint(const WPARAM &wParam, const LPARAM &lParam)
    {
        return (HRESULT)DefWindowProc(Handle, WM_NCPAINT, wParam, lParam);
    }
    virtual LRESULT OnUserMessages(int message, const WPARAM &wParam, const LPARAM &lParam)
    {
        return S_OK;
    }
#if USE_WISP
    virtual void OnCharPress(const WPARAM &wParam, const LPARAM &lParam) {}
#else
    virtual void OnTextInput(const SDL_TextInputEvent &ev) {}
#endif
    virtual void OnKeyDown(const KeyEvent &ev) {}
    virtual void OnKeyUp(const KeyEvent &ev) {}
};

extern CWindow *g_WispWindow;

}; // namespace Wisp

// MIT License

#pragma once

#include <SDL_video.h>
#include "Platform.h"

#define PUSH_EVENT(id, data1, data2) Wisp::CWindow::PushEvent(id, (void *)(data1), (void *)(data2))
#define PLUGIN_EVENT(id, data1, data2)                                                             \
    Wisp::CWindow::PluginEvent(id, (void *)(data1), (void *)(data2))

void GetDisplaySize(int *x, int *y);

namespace Wisp
{
class CWindow
{
#if USE_TIMERTHREAD
    deque<Wisp::CThreadedTimer *> m_ThreadedTimersStack;
#endif // USE_TIMERTHREAD

public:
    bool NoResize = false;

    // FIXME: Last API specific public surface on windowing stuff
    static WindowHandle Handle;
#if USE_WISP
    HINSTANCE hInstance = 0;
    LRESULT OnWindowProc(WindowHandle &hWnd, UINT &message, WPARAM &wParam, LPARAM &lParam);
#else
    SDL_Window *m_window = nullptr;
    bool OnWindowProc(SDL_Event &ev);
#endif

    Wisp::CSize GetSize() { return m_Size; };
    void SetSize(const Wisp::CSize &size);
    Wisp::CSize GetMinSize() { return m_MinSize; };
    void SetMinSize(const Wisp::CSize &newMinSize);
    Wisp::CSize GetMaxSize() { return m_MaxSize; };
    void SetMaxSize(const Wisp::CSize &newMaxSize);
    void GetPositionSize(int *x, int *y, int *width, int *height);
    void SetPositionSize(int x, int y, int width, int height);
    void MaximizeWindow();

public:
    CWindow();
    virtual ~CWindow();

    bool Create(
        const char *className,
        const char *title,
        bool showCursor = false,
        int width = 800,
        int height = 600);
    void Destroy();

    static uint32_t PushEvent(uint32_t id, void *data1 = nullptr, void *data2 = nullptr);
    static uint32_t PluginEvent(uint32_t id, void *data1 = nullptr, void *data2 = nullptr);

    void ShowMessage(const string &text, const string &title);
    void ShowMessage(const wstring &text, const wstring &title);
    void SetMinSize(int width, int height);
    void SetMaxSize(int width, int height);
    bool IsActive() const;
    void SetTitle(const string &text) const;
    void ShowWindow(bool show) const;
    bool IsMinimizedWindow() const;
    bool IsMaximizedWindow() const;
    void CreateTimer(uint32_t id, int delay);
    void RemoveTimer(uint32_t id);
    void Raise();

#if USE_TIMERTHREAD
    CThreadedTimer *CreateThreadedTimer(
        uint32_t id,
        int delay,
        bool oneShot = false,
        bool waitForProcessMessage = true,
        bool synchronizedDelay = false);
    void RemoveThreadedTimer(uint32_t id);
    Wisp::CThreadedTimer *GetThreadedTimer(uint32_t id);
#endif // USE_TIMERTHREAD

protected:
    Wisp::CSize m_Size = Wisp::CSize(640, 480);
    Wisp::CSize m_MinSize = Wisp::CSize(640, 480);
    Wisp::CSize m_MaxSize = Wisp::CSize(640, 480);

    virtual bool OnCreate() { return true; }
    virtual void OnDestroy() {}
    virtual void OnResize() {}
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
    virtual void OnTimer(uint32_t id) {}
#if USE_TIMERTHREAD
    virtual void OnThreadedTimer(uint32_t nowTime, Wisp::CThreadedTimer *timer) {}
#endif // USE_TIMERTHREAD
    virtual void OnSetText(const char *text) {}
    virtual bool OnRepaint(const PaintEvent &ev);
    virtual bool OnUserMessages(const UserEvent &ev) { return true; }
    virtual void OnTextInput(const TextEvent &ev) {}
    virtual void OnKeyDown(const KeyEvent &ev) {}
    virtual void OnKeyUp(const KeyEvent &ev) {}
};

extern CWindow *g_WispWindow;

}; // namespace Wisp

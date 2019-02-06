// MIT License

#ifndef WISP_WIN_H
#define WISP_WIN_H

#include <SDL_video.h>
#include "../Platform.h"
#include "../api/plugininterface.h"

#define PUSH_EVENT(id, data1, data2)                                                               \
    Wisp::CWindow::PushEvent(id, (void *)(uintptr_t)(data1), (void *)(uintptr_t)(data2))
#define PLUGIN_EVENT(id, data) Wisp::CWindow::PluginEvent(id, (const void *)(uintptr_t)(data))

void GetDisplaySize(int *x, int *y);

namespace Wisp
{
class CWindow
{
public:
    // FIXME: Last API specific public surface on windowing stuff
    static WindowHandle Handle;
    SDL_Window *m_window = nullptr;
    bool OnWindowProc(SDL_Event &ev);
    CSize GetSize() { return m_Size; };
    void SetSize(const CSize &size);
    CSize GetMinSize() { return m_MinSize; };
    void SetMinSize(const CSize &newMinSize);
    CSize GetMaxSize() { return m_MaxSize; };
    void SetMaxSize(const CSize &newMaxSize);
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
    static uint32_t PluginEvent(uint32_t id, const void *data = nullptr);

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

protected:
    CSize m_Size = CSize(640, 480);
    CSize m_MinSize = CSize(640, 480);
    CSize m_MaxSize = CSize(640, 480);

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
    virtual void OnSetText(const char *text){};
    virtual bool OnRepaint(const PaintEvent &ev) { return false; };
    virtual bool OnUserMessages(const UserEvent &ev) { return true; }
    virtual void OnTextInput(const TextEvent &ev) {}
    virtual void OnKeyDown(const KeyEvent &ev) {}
    virtual void OnKeyUp(const KeyEvent &ev) {}
};

extern CWindow *g_WispWindow;

}; // namespace Wisp

#endif //WISP_WIN_H

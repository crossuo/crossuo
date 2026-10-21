// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Globals.h"
#include "Platform.h"
#include "Point.h"

enum
{
    FASTLOGIN_TIMER_ID = 1,
    TOUCH_HOLD_TIMER_ID = 2,
};

void GetDisplaySize(int *x, int *y);

// device reports touch input; XUO_EMSC only (false elsewhere)
bool WebIsTouchDevice();

#define PUSH_EVENT(id, data1, data2)                                                               \
    CGameWindow::PushEvent(id, (void *)(uintptr_t)(data1), (void *)(uintptr_t)(data2))

class CGameWindow
{
public:
    // FIXME: Last API specific public surface on windowing stuff
    SDL_Window *m_window = nullptr;
    static const uint32_t MessageID = USER_MESSAGE_ID + 402;

    CGameWindow();
    ~CGameWindow();

    bool OnWindowProc(SDL_Event &ev);
    CSize GetSize() { return m_Size; };
    void SetSize(const CSize &size);
    void GetPositionSize(int *x, int *y, int *width, int *height);
    void SetPositionSize(int x, int y, int width, int height);
    void MaximizeWindow();

    bool Create(const char *title, bool showCursor = false, int width = 800, int height = 600);
    void Destroy();

    static uint32_t PushEvent(uint32_t id, void *data1 = nullptr, void *data2 = nullptr);

    void ShowMessage(const astr_t &text, const astr_t &title);
    void ShowMessage(const wstr_t &text, const wstr_t &title);
    void SetMinSize(int width, int height);
    void SetMaxSize(int width, int height);
    bool IsActive() const;
    void SetTitle(const astr_t &text) const;
    bool IsMinimizedWindow() const;
    bool IsMaximizedWindow() const;
    void CreateTimer(uint32_t id, int delay);
    void RemoveTimer(uint32_t id);
    void Raise();

    void SetRenderTimerDelay(int delay);
    void EmulateOnLeftMouseButtonDown();
    int GetRenderDelay();
    void SetWindowResizable(bool windowResizable) const;
    void RestoreWindow() const;

    bool OnCreate();
    void OnDestroy();
    void OnResize();
    void OnLeftMouseButtonDown();
    void OnLeftMouseButtonUp();
    bool OnLeftMouseButtonDoubleClick();
    void OnRightMouseButtonDown();
    void OnRightMouseButtonUp();
    bool OnRightMouseButtonDoubleClick();
    void OnMidMouseButtonDown();
    void OnMidMouseButtonUp();
    bool OnMidMouseButtonDoubleClick();
    void OnMidMouseButtonScroll(bool up);
    bool OnTouchFinger(const SDL_TouchFingerEvent &ev);
    void OnTouchHoldTimer();
    void TouchTryConvertHold();
    void OnDragging();
    void OnActivate();
    void OnDeactivate();
    void OnTimer(uint32_t id);
    bool OnUserMessages(const UserEvent &ev);
    void OnTextInput(const TextEvent &ev);
    void OnKeyDown(const KeyEvent &ev);
    void OnKeyUp(const KeyEvent &ev);

private:
    CSize m_Size = CSize(640, 480);
    CSize m_MinSize = CSize(640, 480);
    CSize m_MaxSize = CSize(640, 480);
    int m_iRenderDelay = 0;
};

extern CGameWindow g_GameWindow;

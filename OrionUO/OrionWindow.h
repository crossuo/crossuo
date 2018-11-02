// MIT License
// Copyright (C) August 2016 Hotride

#pragma once
#include "Input.h"

class COrionWindow : public Wisp::CWindow
{
    //int RenderTimerDelay = FRAME_DELAY_ACTIVE_WINDOW;

public:
    COrionWindow();
    virtual ~COrionWindow();

    //static const int FASTLOGIN_TIMER_ID = 1;
    static const int RENDER_TIMER_ID = 1;
    static const int UPDATE_TIMER_ID = 2;
    static const int FASTLOGIN_TIMER_ID = 3;

    void SetRenderTimerDelay(int delay);

    void EmulateOnLeftMouseButtonDown();

protected:
    virtual bool OnCreate();
    virtual void OnDestroy();
    virtual void OnResize();
    virtual void OnLeftMouseButtonDown();
    virtual void OnLeftMouseButtonUp();
    virtual bool OnLeftMouseButtonDoubleClick();
    virtual void OnRightMouseButtonDown();
    virtual void OnRightMouseButtonUp();
    virtual bool OnRightMouseButtonDoubleClick();
    virtual void OnMidMouseButtonDown();
    virtual void OnMidMouseButtonUp();
    virtual bool OnMidMouseButtonDoubleClick();
    virtual void OnMidMouseButtonScroll(bool up);
    virtual void OnXMouseButton(bool up);
    virtual void OnDragging();
    virtual void OnActivate();
    virtual void OnDeactivate();
    virtual void OnShow(bool show);
    virtual void OnSetText(const LPARAM &lParam);
    virtual void OnTimer(uint32_t id);
    virtual void OnThreadedTimer(uint32_t nowTime, Wisp::CThreadedTimer *timer);
    virtual HRESULT OnRepaint(const WPARAM &wParam, const LPARAM &lParam);
    virtual bool OnUserMessages(const UserEvent &ev) override;
    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;
    virtual void OnKeyUp(const KeyEvent &ev) override;
};

extern COrionWindow g_OrionWindow;

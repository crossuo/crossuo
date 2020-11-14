// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Platform.h"
#include "Wisp.h"

enum
{
    FASTLOGIN_TIMER_ID = 1,
};

class CGameWindow : public Wisp::CWindow
{
public:
    CGameWindow();
    virtual ~CGameWindow();

    void SetRenderTimerDelay(int delay);
    void EmulateOnLeftMouseButtonDown();
    int GetRenderDelay();
    void SetWindowResizable(bool windowResizable) const;
    void RestoreWindow() const;

    static const uint32_t MessageID = USER_MESSAGE_ID + 402;

protected:
    virtual bool OnCreate() override;
    virtual void OnDestroy() override;
    virtual void OnResize() override;
    virtual void OnLeftMouseButtonDown() override;
    virtual void OnLeftMouseButtonUp() override;
    virtual bool OnLeftMouseButtonDoubleClick() override;
    virtual void OnRightMouseButtonDown() override;
    virtual void OnRightMouseButtonUp() override;
    virtual bool OnRightMouseButtonDoubleClick() override;
    virtual void OnMidMouseButtonDown() override;
    virtual void OnMidMouseButtonUp() override;
    virtual bool OnMidMouseButtonDoubleClick() override;
    virtual void OnMidMouseButtonScroll(bool up) override;
    virtual void OnXMouseButton(bool up) override;
    virtual void OnDragging() override;
    virtual void OnActivate() override;
    virtual void OnDeactivate() override;
    virtual void OnShow(bool show) override;
    virtual void OnSetText(const char *str) override;
    virtual void OnTimer(uint32_t id) override;
    virtual bool OnRepaint(const PaintEvent &ev) override;
    virtual bool OnUserMessages(const UserEvent &ev) override;
    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;
    virtual void OnKeyUp(const KeyEvent &ev) override;

private:
    int m_iRenderDelay;
};

extern CGameWindow g_GameWindow;

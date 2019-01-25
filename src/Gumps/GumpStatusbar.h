// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"
#include "../Platform.h"

class CGumpStatusbar : public CGump
{
protected:
    void SendRenameRequest();

    virtual void CalculateGumpState();

    bool m_WantFullUpdateContent = false;

    CGUIButton *m_StatusbarUnlocker{ nullptr };
    CGUIGumppic *m_Body{ nullptr };
    CGUIGumppic *m_HitsBody{ nullptr };
    CGUITextEntry *m_Entry{ nullptr };

    string m_Name;

public:
    CGumpStatusbar(uint32_t serial, short x, short y, bool minimized);
    virtual ~CGumpStatusbar();

    const int ID_GSB_LOCK_MOVING = 10;

    static int m_StatusbarDefaultWidth;
    static int m_StatusbarDefaultHeight;

    CGumpStatusbar *m_GroupNext{ nullptr };
    CGumpStatusbar *m_GroupPrev{ nullptr };
    CGumpStatusbar *GetTopStatusbar();
    CGumpStatusbar *GetNearStatusbar(int &x, int &y);
    bool GetStatusbarGroupOffset(int &x, int &y);
    void UpdateGroup(int x, int y);
    void AddStatusbar(CGumpStatusbar *bar);
    void RemoveFromGroup();

    bool InGroup() { return (m_GroupNext != nullptr || m_GroupPrev != nullptr); }

    virtual void InitToolTip();
    virtual void PrepareContent();
    virtual void UpdateContent();

    GUMP_BUTTON_EVENT_H;

    virtual void OnLeftMouseButtonDown();
    virtual bool OnLeftMouseButtonDoubleClick();

    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;
};

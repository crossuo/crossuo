// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Gump.h"
#include "../GUI/GUITextEntry.h"

class CGumpChat : public CGump
{
private:
    static const int ID_GC_NONE = 0;
    static const int ID_GC_JOIN = 1;
    static const int ID_GC_LEAVE = 2;
    static const int ID_GC_CREATE = 3;
    static const int ID_GC_HTML_GUMP = 4;
    static const int ID_GC_CB_ENTRY = 5;
    static const int ID_GC_CB_OKAY = 6;
    static const int ID_GC_CB_CLOSE = 7;
    static const int ID_GC_MSG_ENTRY = 8;

    //Base serial for the channel row hit boxes (serial = base + row index)
    static const int ID_GC_CHANNEL = 0x0100;

    static const int GUMP_WIDTH = 345;
    static const int GUMP_HEIGHT = 390;

    CGUITextEntry *m_Entry{ nullptr };
    bool m_CreationBoxOpen{ false };
    int m_SelectedChannel{ -1 };
    astr_t m_EntryText = {};
    CGUITextEntry *m_MessageEntry{ nullptr };
    astr_t m_MessageText = {};

    bool FocusedEntryIs(int which) const;
    void SendCurrentMessage();

    void UpdateCreationBoxText();

public:
    CGumpChat(int x, int y);
    virtual ~CGumpChat();

    virtual void UpdateContent() override;
    virtual void OnLeftMouseButtonUp() override;

    GUMP_BUTTON_EVENT_H override;

    // Automation hooks (chat test harness): route a synthetic button click
    // through the same dispatch a real click uses, and fill + confirm the
    // channel-creation box
    void DebugClick(int id);
    void DebugCreateChannel(const astr_t &name);
    void DebugSendMessage(const astr_t &name);
    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;
};

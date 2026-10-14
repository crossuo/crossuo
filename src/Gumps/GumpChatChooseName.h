// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Gump.h"
#include "../GUI/GUITextEntry.h"

class CGumpChatChooseName : public CGump
{
private:
    static const int ID_GCCN_NONE = 0;
    static const int ID_GCCN_ENTRY = 1;
    static const int ID_GCCN_OKAY = 2;
    static const int ID_GCCN_CLOSE = 3;

    CGUITextEntry *m_Entry{ nullptr };

public:
    CGumpChatChooseName(int x, int y);
    virtual ~CGumpChatChooseName();

    virtual void UpdateContent() override;
    virtual void OnLeftMouseButtonUp() override;

    GUMP_BUTTON_EVENT_H override;
    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;
};

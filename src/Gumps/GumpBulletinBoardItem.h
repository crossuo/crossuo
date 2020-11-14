// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "GumpBaseScroll.h"
#include "../Platform.h"

class CGumpBulletinBoardItem : public CGumpBaseScroll
{
protected:
    uint8_t m_Variant{ 0 };
    CGUIButton *m_ButtonPost{ nullptr };
    CGUIButton *m_ButtonRemove{ nullptr };
    CGUIButton *m_ButtonReply{ nullptr };
    CGUIHitBox *m_HitBox{ nullptr };

    void RecalculateHeight();
    virtual void UpdateHeight() override;

public:
    CGumpBulletinBoardItem(
        int serial,
        int x,
        int y,
        uint8_t variant,
        int id,
        const wstr_t &poster,
        const wstr_t &subject,
        const wstr_t &dataTime,
        const wstr_t &data);
    virtual ~CGumpBulletinBoardItem();

    CGUITextEntry *m_EntrySubject{ nullptr };
    CGUITextEntry *m_Entry{ nullptr };

    GUMP_BUTTON_EVENT_H override;

    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;
};

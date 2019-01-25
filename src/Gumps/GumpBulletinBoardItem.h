// MIT License
// Copyright (C) August 2016 Hotride

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
    virtual void UpdateHeight();

public:
    CGumpBulletinBoardItem(
        int serial,
        int x,
        int y,
        uint8_t variant,
        int id,
        const wstring &poster,
        const wstring &subject,
        const wstring &dataTime,
        const wstring &data);
    virtual ~CGumpBulletinBoardItem();

    CGUITextEntry *m_EntrySubject{ nullptr };
    CGUITextEntry *m_Entry{ nullptr };

    GUMP_BUTTON_EVENT_H;

    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;
};

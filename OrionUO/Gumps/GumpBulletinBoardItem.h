/***********************************************************************************
**
** GumpBulletinBoardItem.h
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#pragma once
#include "Input.h"

class CGumpBulletinBoardItem : public CGumpBaseScroll
{
protected:
    static const int ID_GBBI_SUBJECT_TEXT_FIELD = 1;
    static const int ID_GBBI_TEXT_FIELD = 2;
    static const int ID_GBBI_POST = 3;
    static const int ID_GBBI_REPLY = 4;
    static const int ID_GBBI_REMOVE = 5;

    uint8_t m_Variant{ 0 };

    CGUIButton *m_ButtonPost{ nullptr };
    CGUIButton *m_ButtonRemove{ nullptr };
    CGUIButton *m_ButtonReply{ nullptr };

    CGUIHitBox *m_HitBox{ nullptr };

    void RecalculateHeight();

protected:
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

#if USE_WISP
    virtual void OnCharPress(const WPARAM &wParam, const LPARAM &lParam);
#else
    virtual void OnTextInput(const SDL_TextInputEvent &ev) override;
#endif
    virtual void OnKeyDown(const KeyEvent &ev) override;
};

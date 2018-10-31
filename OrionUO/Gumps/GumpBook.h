/***********************************************************************************
**
** GumpBook.h
**
** Copyright (C) September 2016 Hotride
**
************************************************************************************
*/

#pragma once
#include "Input.h"

class CGumpBook : public CGump
{
public:
    bool Writable = false;
    short PageCount = 0;
    bool Unicode = false;

private:
    static const int ID_GB_TEXT_AREA_AUTHOR = 1;
    static const int ID_GB_TEXT_AREA_TITLE = 2;
    static const int ID_GB_TEXT_AREA_PAGE_LEFT = 3;
    static const int ID_GB_TEXT_AREA_PAGE_RIGHT = 4;
    static const int ID_GB_BUTTON_PREV = 5;
    static const int ID_GB_BUTTON_NEXT = 6;

    bool *m_ChangedPage{ nullptr };
    bool *m_PageDataReceived{ nullptr };
	bool WasAtEnd = false;

    void InsertInContent(const Keycode key, bool isCharPress = true);

    CGUIButton *m_PrevPage{ nullptr };
    CGUIButton *m_NextPage{ nullptr };

    void SetPagePos(int val, int page);

public:
    CGumpBook(uint serial, short x, short y, short pageCount, bool writable, bool unicode);
    virtual ~CGumpBook();

    virtual void PasteClipboardData(wstring &data) override;

    CGUITextEntry *m_EntryAuthor{ nullptr };
    CGUITextEntry *m_EntryTitle{ nullptr };

    CGUITextEntry *GetEntry(int page);

    virtual void DelayedClick(CRenderObject *obj);

    virtual void PrepareContent();

    void SetPageData(int page, const wstring &data);

    void ChangePage(int newPage, bool playSound = true);

    GUMP_BUTTON_EVENT_H;

    bool OnLeftMouseButtonDoubleClick();

#if USE_WISP
    void OnCharPress(const WPARAM &wParam, const LPARAM &lParam);
#else
    virtual void OnTextInput(const SDL_TextInputEvent &ev) override;
#endif
    virtual void OnKeyDown(const KeyEvent &ev) override;
};

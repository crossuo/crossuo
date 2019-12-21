// MIT License
// Copyright (C) September 2016 Hotride

#pragma once

#include "Gump.h"
#include "../Platform.h"

class CGumpBook : public CGump
{
private:
    bool *m_ChangedPage{ nullptr };
    bool *m_PageDataReceived{ nullptr };
    bool WasAtEnd = false;

    void InsertInContent(const Keycode key, bool isCharPress = true);

    CGUIButton *m_PrevPage{ nullptr };
    CGUIButton *m_NextPage{ nullptr };

    void SetPagePos(int val, int page);

public:
    uint16_t PageCount = 0;
    bool Writable = false;
    bool Unicode = false;

    CGumpBook(
        uint32_t serial, int16_t x, int16_t y, uint16_t pageCount, bool writable, bool unicode);
    virtual ~CGumpBook();

    virtual void PasteClipboardData(wstr_t &data) override;

    CGUITextEntry *m_EntryAuthor{ nullptr };
    CGUITextEntry *m_EntryTitle{ nullptr };
    CGUITextEntry *GetEntry(int page);

    virtual void DelayedClick(CRenderObject *obj) override;
    virtual void PrepareContent() override;

    void SetPageData(int page, const wstr_t &data);
    void ChangePage(int newPage, bool playSound = true);

    GUMP_BUTTON_EVENT_H override;

    virtual bool OnLeftMouseButtonDoubleClick() override;

    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;
};

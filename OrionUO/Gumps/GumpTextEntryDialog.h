/***********************************************************************************
**
** GumpTextEntryDialog.h
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#pragma once
#include "Input.h"

class CGumpTextEntryDialog : public CGump
{
public:
    uint8_t Variant = 0;
    uint8_t ParentID = 0;
    uint8_t ButtonID = 0;

private:
    static constexpr int ID_GTED_BUTTON_OKAY = 1;
    static constexpr int ID_GTED_BUTTON_CANCEL = 2;
    static constexpr int ID_GTED_TEXT_FIELD = 3;

    string Text = string("");
    string m_Description = string("");
    int m_MaxLength{ 0 };

    CGUIGumppic *m_TextField{ nullptr };
    CGUITextEntry *m_Entry{ nullptr };

public:
    CGumpTextEntryDialog(
        uint32_t serial,
        short x,
        short y,
        uint8_t variant,
        int maxLength,
        string text,
        string description);
    virtual ~CGumpTextEntryDialog();

    void SendTextEntryDialogResponse(bool mode);

    virtual void PrepareContent();

    virtual void UpdateContent();

    GUMP_BUTTON_EVENT_H;

#if USE_WISP
    void OnCharPress(const WPARAM &wParam, const LPARAM &lParam);
#else
    virtual void OnTextInput(const SDL_TextInputEvent &ev) override;
#endif
    virtual void OnKeyDown(const KeyEvent &ev) override;
};

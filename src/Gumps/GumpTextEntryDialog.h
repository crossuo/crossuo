// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Gump.h"
#include "../Platform.h"

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

    std::string Text;
    std::string m_Description;
    int m_MaxLength = 0;

    CGUIGumppic *m_TextField = nullptr;
    CGUITextEntry *m_Entry = nullptr;

public:
    CGumpTextEntryDialog(
        uint32_t serial,
        short x,
        short y,
        uint8_t variant,
        int maxLength,
        std::string text,
        std::string description);
    virtual ~CGumpTextEntryDialog();

    void SendTextEntryDialogResponse(bool mode);

    virtual void PrepareContent() override;
    virtual void UpdateContent() override;

    GUMP_BUTTON_EVENT_H override;

    virtual void OnTextInput(const TextEvent &ev) override;
    virtual void OnKeyDown(const KeyEvent &ev) override;
};

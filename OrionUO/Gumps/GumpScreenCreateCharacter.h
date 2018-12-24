// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CGumpScreenCreateCharacter : public CGump
{
    enum
    {
        CCSID_HAIR_STYLE = 1,
        CCSID_FACIAL_HAIR_STYLE,
        CCSID_SKIN_TONE = 1,
        CCSID_SHIRT_COLOR,
        CCSID_SKIRT_OR_PANTS_COLOR,
        CCSID_HAIR_COLOR,
        CCSID_FACIAL_HAIR_COLOR
    };

    CGUIGumppic *m_Hair{ nullptr };
    CGUIGumppic *m_FacialHair{ nullptr };

public:
    CGumpScreenCreateCharacter();
    virtual ~CGumpScreenCreateCharacter();

    virtual void UpdateContent() override;
    virtual void InitToolTip() override;

    GUMP_BUTTON_EVENT_H;
    GUMP_RADIO_EVENT_H;
    GUMP_TEXT_ENTRY_EVENT_H;
    GUMP_COMBOBOX_SELECTION_EVENT_H;
};

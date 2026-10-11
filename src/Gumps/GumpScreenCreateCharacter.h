// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Gump.h"
#include "Gump1998ScrollChoice.h"
#include <vector>

class CGUIText;
class CGUISlider;
class CGUIGumppic;
class CGUIButton;

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

private:
    //1.25.32 (1998) InitSkill step: three skill picks
    void UpdateContent1998();

    //the skill popup remembers which of the three picks opened it
    //1.25.32 (1998) CharCreation: the open popup (0 none, 1 hair, 2 beard)
    int m_1998OpenPopup = 0;
    CGUISlider *m_1998HueSliders[6];
    CGUIText *m_1998StyleName[2];
    CGUIGumppic *m_1998PreviewBody = nullptr;
    CGUIGumppic *m_1998PreviewHair = nullptr;
    CGUIGumppic *m_1998PreviewBeard = nullptr;
    C1998ScrollChoice m_1998HairPopup;
    C1998ScrollChoice m_1998BeardPopup;

public:
    CGumpScreenCreateCharacter();
    virtual ~CGumpScreenCreateCharacter();

    virtual void UpdateContent() override;
    virtual void InitToolTip() override;

    GUMP_BUTTON_EVENT_H override;
    GUMP_RADIO_EVENT_H override;
    GUMP_TEXT_ENTRY_EVENT_H override;
    GUMP_COMBOBOX_SELECTION_EVENT_H override;
    GUMP_SLIDER_CLICK_EVENT_H override;
    GUMP_SLIDER_MOVE_EVENT_H override;
};

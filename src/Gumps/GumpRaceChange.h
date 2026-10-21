// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Gump.h"
#include "../Globals.h" // RACE_TYPE

class CGumpRaceChange : public CGump
{
protected:
    bool m_Female = false;
    RACE_TYPE m_Race = RT_HUMAN;
    int m_HairStyle = 1;
    int m_BeardStyle = 0;
    uint16_t m_SkinTone = 0;
    uint16_t m_HairColor = 0;
    uint16_t m_BeardColor = 0;

    //!which color row's palette is open (0 = none, otherwise a row id)
    int m_ColorSelection = 0;

    //!the character creation tables are shared with the creation screen, so
    //!the manager state is borrowed for the gump lifetime and restored after
    RACE_TYPE m_SavedRace = RT_HUMAN;
    bool m_SavedFemale = false;
    uint8_t m_SavedHairStyle = 0;
    uint8_t m_SavedBeardStyle = 0;
    uint16_t m_SavedSkinTone = 0;
    uint16_t m_SavedShirtColor = 0;
    uint16_t m_SavedPantsColor = 0;
    uint16_t m_SavedHairColor = 0;
    uint16_t m_SavedBeardColor = 0;

    bool m_Borrowed = false;

    void RestoreCharacterCreationState();
    uint16_t GetPreviewBodyGump() const;
    void AddColorRow(int x, int y, int id, const char *label, uint16_t selectedHue);
    void AddColorPicker(int idBase, const uint16_t *palette, int gridYOffset);

public:
    CGumpRaceChange(bool isFemale, uint8_t race);
    virtual ~CGumpRaceChange() override;

    virtual void UpdateContent() override;

    GUMP_BUTTON_EVENT_H override;
    GUMP_TEXT_ENTRY_EVENT_H override;
    GUMP_COMBOBOX_SELECTION_EVENT_H override;
};

extern CGumpRaceChange *g_GumpRaceChange;

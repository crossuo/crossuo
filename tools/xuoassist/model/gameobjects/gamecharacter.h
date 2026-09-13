// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (gameobjects/gamecharacter.h): strings are std::string.

#pragma once

#include "gameobject.h"

namespace xuoassist {

//! Game character
class CGameCharacter : public CGameObject
{
    SETGET(short, Hits, 0)
    SETGET(short, MaxHits, 0)
    SETGET(short, Mana, 0)
    SETGET(short, MaxMana, 0)
    SETGET(short, Stam, 0)
    SETGET(short, MaxStam, 0)
    SETGET(bool, Sex, false)
    SETGET(RACE_TYPE, Race, RT_HUMAN)
    SETGET(uchar, Direction, 0)
    SETGET(uchar, Notoriety, 0)
    SETGET(bool, CanChangeName, false)
    SETGET(bool, ProfileReceived, false)
    SETGET(bool, BlockProfile, false)
    SETGET(string, Profile, "")
    SETGET(string, Title, "")

public:
    CGameCharacter(const uint &serial = 0);
    virtual ~CGameCharacter();

    bool IsHuman()
    {
        return (
            IN_RANGE(m_Graphic, 0x0190, 0x0193) || IN_RANGE(m_Graphic, 0x00B7, 0x00BA) ||
            IN_RANGE(m_Graphic, 0x025D, 0x0260) || IN_RANGE(m_Graphic, 0x029A, 0x029B) ||
            IN_RANGE(m_Graphic, 0x02B6, 0x02B7) || (m_Graphic == 0x03DB) || (m_Graphic == 0x03DF) ||
            (m_Graphic == 0x03E2));
    }

    bool IsGM() { return (m_Graphic == 0x03DB); }

    bool Dead()
    {
        return (
            IN_RANGE(m_Graphic, 0x0192, 0x0193) || IN_RANGE(m_Graphic, 0x025F, 0x0260) ||
            IN_RANGE(m_Graphic, 0x02B6, 0x02B7));
    }
};

} // namespace xuoassist


// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (gameobjects/gameplayer.h).

#ifndef XUOASSIST_GAMEPLAYER_H
#define XUOASSIST_GAMEPLAYER_H

#include "gamecharacter.h"

namespace xuoassist {

//! Класс игрока
class CPlayer : public CGameCharacter
{
    SETGET(short, Str, 0)
    SETGET(short, Int, 0)
    SETGET(short, Dex, 0)
    SETGET(uchar, LockStr, 0)
    SETGET(uchar, LockInt, 0)
    SETGET(uchar, LockDex, 0)
    SETGET(ushort, MaxWeight, 0)
    SETGET(ushort, Weight, 0)
    SETGET(short, Armor, 0)
    SETGET(uint, Gold, 0)
    SETGET(bool, Warmode, false)
    SETGET(ushort, StatsCap, 0)
    SETGET(uchar, Followers, 0)
    SETGET(uchar, MaxFollowers, 5)
    SETGET(short, FireResistance, 0)
    SETGET(short, ColdResistance, 0)
    SETGET(short, PoisonResistance, 0)
    SETGET(short, EnergyResistance, 0)
    SETGET(short, Luck, 0)
    SETGET(short, MinDamage, 0)
    SETGET(short, MaxDamage, 0)
    SETGET(uint, TithingPoints, 0)
    SETGET(int, StealthSteps, 0)

public:
    CPlayer(const uint &serial);
    virtual ~CPlayer();

    class CGameItem *FindBandage();

    bool IsPlayer() { return true; }

    void UpdateAbilities();
};

//! Ссылка на игрока
extern CPlayer *g_Player;

} // namespace xuoassist

#endif // XUOASSIST_GAMEPLAYER_H

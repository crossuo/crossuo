// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <stdint.h>
#include <common/str.h>

struct CDataReader;
class CGameCharacter;

struct CPartyObject
{
    uint32_t Serial = 0;
    CGameCharacter *Character = nullptr;
    // Last known position for the map window (kept for members that left the
    // view range or are only known from a party-info packet)
    int X = 0;
    int Y = 0;
    uint8_t MapIndex = 0;
    astr_t Name;
    astr_t GetName(int index);
};

struct CParty
{
    uint32_t Leader = 0;
    uint32_t Inviter = 0;
    bool CanLoot = false;
    CPartyObject Member[10];

    void ParsePacketData(CDataReader &reader);
    bool Contains(int serial);
    // Refresh the stored position/name of a party member from its world object
    void SaveMemberInfo(CGameCharacter *obj);
    // Store externally supplied party-info packet member data; returns whether
    // the serial belongs to the party
    bool SetMemberInfo(uint32_t serial, int x, int y, uint8_t mapIndex, const astr_t &name);
    void Clear();
};

extern CParty g_Party;

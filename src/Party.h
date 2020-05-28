// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include <stdint.h>
#include <common/str.h>

struct CDataReader;
class CGameCharacter;

struct CPartyObject
{
    uint32_t Serial = 0;
    CGameCharacter *Character = nullptr;
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
    void Clear();
};

extern CParty g_Party;

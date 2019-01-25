// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "PartyObject.h"

class CParty
{
public:
    uint32_t Leader = 0;
    uint32_t Inviter = 0;
    bool CanLoot = false;
    CPartyObject Member[10];

    CParty();
    ~CParty();

    void ParsePacketData(Wisp::CDataReader &reader);
    bool Contains(int serial);
    void Clear();
};

extern CParty g_Party;

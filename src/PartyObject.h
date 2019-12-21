// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include <stdint.h>
#include <common/str.h>

class CGameCharacter;

class CPartyObject
{
public:
    uint32_t Serial = 0;

    CPartyObject();
    ~CPartyObject() {}
    CGameCharacter *Character{ nullptr };
    astr_t GetName(int index);
};

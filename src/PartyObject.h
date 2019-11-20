// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "CrossPCH.h" // REMOVE

class CPartyObject
{
public:
    uint32_t Serial = 0;

    CPartyObject();
    ~CPartyObject() {}
    class CGameCharacter *Character{ nullptr };
    std::string GetName(int index);
};

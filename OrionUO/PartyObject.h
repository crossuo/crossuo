// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CPartyObject
{
public:
    uint32_t Serial = 0;

    CPartyObject();
    ~CPartyObject() {}
    class CGameCharacter *Character{ nullptr };
    string GetName(int index);
};

// MIT License
// Copyright (C) August 2016 Hotride

#include "PartyObject.h"
#include "GameObjects/GameWorld.h"
#include "GameObjects/GameCharacter.h"

CPartyObject::CPartyObject()
{
}

std::string CPartyObject::GetName(int index)
{
    if (Serial != 0u)
    {
        if (Character == nullptr)
        {
            Character = g_World->FindWorldCharacter(Serial);
        }
        if (Character != nullptr)
        {
            return Character->GetName();
        }
    }

    char buf[10] = { 0 };
    sprintf(buf, "[%i]", index);

    return std::string(buf);
}

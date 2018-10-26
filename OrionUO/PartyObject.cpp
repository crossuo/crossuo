// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** PartyObject.cpp
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#include "stdafx.h"

CPartyObject::CPartyObject()
{
}

string CPartyObject::GetName(int index)
{
    DEBUG_TRACE_FUNCTION;
    if (Serial)
    {
        if (Character == nullptr)
            Character = g_World->FindWorldCharacter(Serial);
        if (Character != nullptr)
            return Character->GetName();
    }

    char buf[10] = { 0 };
    sprintf_s(buf, "[%i]", index);

    return string(buf);
}


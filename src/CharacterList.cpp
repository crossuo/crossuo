﻿// MIT License

#include "CharacterList.h"

CCharacterList g_CharacterList;

CCharacterList::CCharacterList()
{
    Clear();
}

void CCharacterList::Clear()
{
    for (int i = 0; i < 7; i++)
    {
        m_Name[i] = "";
    }

    Selected = 0;
    OnePerson = false;
    Have6Slot = false;
    Have7Slot = false;
}

void CCharacterList::SetName(intptr_t pos, const std::string &name)
{
    if (pos >= 0 && pos < Count)
    {
        m_Name[pos] = name;
    }
}

std::string CCharacterList::GetName(intptr_t pos) const
{
    if (pos >= 0 && pos < Count)
    {
        return m_Name[pos];
    }

    return "";
}

std::string CCharacterList::GetSelectedName() const
{
    if (Selected >= 0 && Selected < Count)
    {
        return m_Name[Selected];
    }

    return "";
}

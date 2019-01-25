// MIT License

#include "CharacterList.h"

CCharacterList g_CharacterList;

CCharacterList::CCharacterList()
{
    DEBUG_TRACE_FUNCTION;
    Clear();
}

void CCharacterList::Clear()
{
    DEBUG_TRACE_FUNCTION;
    for (int i = 0; i < 7; i++)
    {
        m_Name[i] = "";
    }

    Selected = 0;
    OnePerson = false;
    Have6Slot = false;
    Have7Slot = false;
}

void CCharacterList::SetName(intptr_t pos, const string &name)
{
    DEBUG_TRACE_FUNCTION;
    if (pos >= 0 && pos < Count)
    {
        m_Name[pos] = name;
    }
}

string CCharacterList::GetName(intptr_t pos) const
{
    DEBUG_TRACE_FUNCTION;
    if (pos >= 0 && pos < Count)
    {
        return m_Name[pos];
    }

    return "";
}

string CCharacterList::GetSelectedName() const
{
    DEBUG_TRACE_FUNCTION;
    if (Selected >= 0 && Selected < Count)
    {
        return m_Name[Selected];
    }

    return "";
}

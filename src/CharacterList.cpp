// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "CharacterList.h"

CCharacterList g_CharacterList;
static astr_t s_invalid;

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
    SingleSlot = false;
    Have6Slot = false;
    Have7Slot = false;
}

void CCharacterList::SetName(intptr_t pos, const astr_t &name)
{
    if (pos >= 0 && pos < Count)
    {
        m_Name[pos] = name;
    }
}

const astr_t &CCharacterList::GetName(intptr_t pos) const
{
    if (pos >= 0 && pos < Count)
    {
        return m_Name[pos];
    }
    return s_invalid;
}

const astr_t &CCharacterList::GetSelectedName() const
{
    if (Selected >= 0 && Selected < Count)
    {
        return m_Name[Selected];
    }
    return s_invalid;
}

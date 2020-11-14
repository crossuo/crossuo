// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride
// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#pragma once

#include <common/str.h>

class CCharacterList
{
    astr_t m_Name[7];

public:
    int Count = 5;
    int Selected = 0;
    bool SingleSlot = false;
    bool Have6Slot = false;
    bool Have7Slot = false;
    astr_t LastCharacterName;

    CCharacterList();
    virtual ~CCharacterList() = default;
    void Clear();
    void SetName(intptr_t pos, const astr_t &name);
    const astr_t &GetName(intptr_t pos) const;
    const astr_t &GetSelectedName() const;
};

extern CCharacterList g_CharacterList;

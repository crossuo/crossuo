// MIT License

#pragma once

#include <common/str.h>

class CCharacterList
{
public:
    int Count = 5;
    int Selected = 0;
    bool OnePerson = false;
    bool Have6Slot = false;
    bool Have7Slot = false;

    astr_t LastCharacterName;

private:
    astr_t m_Name[7];

public:
    CCharacterList();
    virtual ~CCharacterList() {}

    void Clear();
    void SetName(intptr_t pos, const astr_t &name);
    astr_t GetName(intptr_t pos) const;
    astr_t GetSelectedName() const;
};

extern CCharacterList g_CharacterList;

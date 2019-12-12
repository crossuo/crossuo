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

    std::string LastCharacterName;

private:
    std::string m_Name[7];

public:
    CCharacterList();
    virtual ~CCharacterList() {}

    void Clear();
    void SetName(intptr_t pos, const std::string &name);
    std::string GetName(intptr_t pos) const;
    std::string GetSelectedName() const;
};

extern CCharacterList g_CharacterList;

// MIT License

#pragma once

class CCharacterList
{
public:
    int Count = 5;
    int Selected = 0;
    bool OnePerson = false;
    bool Have6Slot = false;
    bool Have7Slot = false;

    string LastCharacterName;

private:
    string m_Name[7];

public:
    CCharacterList();
    virtual ~CCharacterList() {}

    void Clear();
    void SetName(intptr_t pos, const string &name);
    string GetName(intptr_t pos) const;
    string GetSelectedName() const;
};

extern CCharacterList g_CharacterList;

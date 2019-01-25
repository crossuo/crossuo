// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CSkillGroupObject
{
public:
    int Count = 0;
    bool Maximized = false;
    string Name = "No Name";

private:
    uint8_t m_Items[60];

public:
    CSkillGroupObject *m_Next{ nullptr };
    CSkillGroupObject *m_Prev{ nullptr };

    CSkillGroupObject();
    ~CSkillGroupObject();

    uint8_t GetItem(intptr_t index);
    void Add(uint8_t index);
    void AddSorted(uint8_t index);
    void Remove(uint8_t index);
    bool Contains(uint8_t index);
    void Sort();
    void TransferTo(CSkillGroupObject *group);
};

// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CFastWalkStack
{
private:
    uint32_t m_Keys[MAX_FAST_WALK_STACK_SIZE];

public:
    CFastWalkStack() { memset(m_Keys, 0, sizeof(m_Keys)); }
    ~CFastWalkStack() {}

    void SetValue(int index, int value);
    void AddValue(int value);
    uint32_t GetValue();
};

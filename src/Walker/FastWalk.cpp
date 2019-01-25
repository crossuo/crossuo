// MIT License
// Copyright (C) August 2016 Hotride

#include "FastWalk.h"

void CFastWalkStack::SetValue(int index, int value)
{
    DEBUG_TRACE_FUNCTION;
    if (index >= 0 && index < 5)
    {
        m_Keys[index] = value;
    }
}

void CFastWalkStack::AddValue(int value)
{
    DEBUG_TRACE_FUNCTION;

    for (int i = 0; i < 5; i++)
    {
        if (m_Keys[i] == 0u)
        {
            m_Keys[i] = value;
            break;
        }
    }
}

uint32_t CFastWalkStack::GetValue()
{
    DEBUG_TRACE_FUNCTION;

    for (int i = 0; i < 5; i++)
    {
        uint32_t key = m_Keys[i];

        if (key != 0u)
        {
            m_Keys[i] = 0;
            return key;
        }
    }

    return 0;
}

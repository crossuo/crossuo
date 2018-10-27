// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** FastWalk.cpp
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#include "stdafx.h"

void CFastWalkStack::SetValue(int index, int value)
{
    DEBUG_TRACE_FUNCTION;
    if (index >= 0 && index < 5)
        m_Keys[index] = value;
}

void CFastWalkStack::AddValue(int value)
{
    DEBUG_TRACE_FUNCTION;

    for (int i = 0; i < 5; i++)
    {
        if (!m_Keys[i])
        {
            m_Keys[i] = value;
            break;
        }
    }
}

uint CFastWalkStack::GetValue()
{
    DEBUG_TRACE_FUNCTION;

    for (int i = 0; i < 5; i++)
    {
        uint key = m_Keys[i];

        if (key)
        {
            m_Keys[i] = 0;
            return key;
        }
    }

    return 0;
}


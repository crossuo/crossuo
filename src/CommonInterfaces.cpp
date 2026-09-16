// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include <string.h>
#include <xuocore/commoninterfaces.h>

IGameString::IGameString()
    : m_Unicode(false)
    , m_DataA(nullptr)
    , m_DataW(nullptr)
{
}

IGameString::~IGameString()
{
    if (m_DataA)
        delete m_DataA;
    m_DataA = nullptr;
    if (m_DataW)
        delete m_DataW;
    m_DataW = nullptr;
}

IGameString &IGameString::operator()(const astr_t &str)
{
    if (m_DataA)
        delete m_DataA;
    m_DataA = nullptr;

    m_Unicode = false;
    if (str.length() != 0u)
    {
        m_DataA = new char[str.length() + 1];
        memcpy(&m_DataA[0], &str[0], str.length());
        m_DataA[str.length()] = 0;
    }

    return *this;
}

IGameString &IGameString::operator()(const wstr_t &str)
{
    if (m_DataW)
        delete m_DataW;
    m_DataW = nullptr;

    m_Unicode = true;
    if (str.length() != 0u)
    {
        m_DataW = new wchar_t[str.length() + 1];
        memcpy(&m_DataW[0], &str[0], str.length() * 2);
        m_DataW[str.length()] = 0;
    }

    return *this;
}

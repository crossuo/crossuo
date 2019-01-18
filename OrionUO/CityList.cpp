// MIT License
// Copyright (C) August 2016 Hotride

#include "CityList.h"
#include "Managers/CityManager.h"

CCityList g_CityList;

CCityItem::CCityItem()
    : m_City("", {})
{
}

CCityItem::~CCityItem()
{
}

void CCityItem::InitCity()
{
    DEBUG_TRACE_FUNCTION;
    m_City = g_CityManager.GetCity(Name);
}

CCityItemNew::CCityItemNew()
    : CCityItem()
{
}

CCityItemNew::~CCityItemNew()
{
}

CCityList::CCityList()
{
}

CCityList::~CCityList()
{
    DEBUG_TRACE_FUNCTION;
    Clear();
}

CCityItem *CCityList::GetCity(int index)
{
    DEBUG_TRACE_FUNCTION;

    if (index < (int)m_CityList.size())
    {
        return m_CityList[index];
    }

    return nullptr;
}

void CCityList::Clear()
{
    DEBUG_TRACE_FUNCTION;

    for (auto i = m_CityList.begin(); i != m_CityList.end(); ++i)
    {
        delete *i;
    }

    m_CityList.clear();
}

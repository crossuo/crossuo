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
    Clear();
}

CCityItem *CCityList::GetCity(int index)
{
    if (index < (int)m_CityList.size())
    {
        return m_CityList[index];
    }

    return nullptr;
}

void CCityList::Clear()
{
    for (auto i = m_CityList.begin(); i != m_CityList.end(); ++i)
    {
        delete *i;
    }

    m_CityList.clear();
}

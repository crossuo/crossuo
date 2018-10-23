// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** CityList.cpp
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#include "stdafx.h"

CCityList g_CityList;

//--------------------------------------CCityItem-----------------------------------

CCityItem::CCityItem()
    : m_City("", L"")
{
}

CCityItem::~CCityItem()
{
}

/*!
Инициализация
@return 
*/
void CCityItem::InitCity()
{
    DEBUG_TRACE_FUNCTION;
    //!Линкуем город
    m_City = g_CityManager.GetCity(Name);
}

//-------------------------------------CCityItemNew---------------------------------

CCityItemNew::CCityItemNew()
    : CCityItem()
{
}

CCityItemNew::~CCityItemNew()
{
}

//--------------------------------------CCityList-----------------------------------

CCityList::CCityList()
{
}

CCityList::~CCityList()
{
    DEBUG_TRACE_FUNCTION;
    Clear();
}

/*!
Получить ссылку на город
@param [__in] index Индекс города
@return
*/
CCityItem *CCityList::GetCity(int index)
{
    DEBUG_TRACE_FUNCTION;

    if (index < m_CityList.size())
        return m_CityList[index];

    return NULL;
}

/*!
Получить ссылку на город
@param [__in] index Индекс города
@return
*/
void CCityList::Clear()
{
    DEBUG_TRACE_FUNCTION;
    for (vector<CCityItem *>::iterator i = m_CityList.begin(); i != m_CityList.end(); ++i)
        delete *i;

    m_CityList.clear();
}

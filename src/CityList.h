// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Managers/CityManager.h"

class CCityItem
{
public:
    uint8_t LocationIndex = 0;
    string Name = "";
    string Area = "";
    bool Selected = false;

    CCityItem();
    virtual ~CCityItem();

    CCity m_City;

    virtual bool IsNewCity() { return false; }
    void InitCity();
};

class CCityItemNew : public CCityItem
{
public:
    uint32_t X = 0;
    uint32_t Y = 0;
    uint32_t Z = 0;
    uint32_t MapIndex = 0;
    uint32_t Cliloc = 0;

    CCityItemNew();
    virtual ~CCityItemNew();

    bool IsNewCity() { return true; }
};

class CCityList
{
private:
    vector<CCityItem *> m_CityList;

public:
    CCityList();
    virtual ~CCityList();

    size_t CityCount() { return m_CityList.size(); }
    void AddCity(CCityItem *city) { m_CityList.push_back(city); }
    CCityItem *GetCity(int index);
    void Clear();
};

extern CCityList g_CityList;

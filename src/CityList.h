// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "Managers/CityManager.h"

class CCityItem
{
public:
    uint8_t LocationIndex = 0;
    astr_t Name = "";
    astr_t Area = "";
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
    std::vector<CCityItem *> m_CityList;

public:
    CCityList();
    virtual ~CCityList();

    size_t CityCount() { return m_CityList.size(); }
    void AddCity(CCityItem *city) { m_CityList.push_back(city); }
    CCityItem *GetCity(int index);
    void Clear();

    //!1.25.32 (1998): the start-location list the CityChoice scroll shows is
    //!NOT server-driven - the original loads it from regions.mul at startup
    //!(FUN_0046B1D0, and again from the 0x57 push). Fills the list when it
    //!is still empty; falls back to the classic nine-city table when the
    //!file is absent so the scroll is never empty. Returns the row count.
    size_t LoadRegionsOrDefault();
};

extern CCityList g_CityList;

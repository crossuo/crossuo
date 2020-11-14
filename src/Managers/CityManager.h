// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <stdint.h>
#include <common/str.h>
#include <queue>

class CCity
{
public:
    astr_t Name = {};
    wstr_t Description = {};

    CCity(const astr_t &name, const wstr_t &description);
    CCity() = default;
    virtual ~CCity();
};

class CCityManager
{
public:
    std::deque<CCity> m_CityList;

    CCityManager();
    virtual ~CCityManager();

    void Init();
    void Clear();
    CCity GetCity(const astr_t &name);
};

extern CCityManager g_CityManager;

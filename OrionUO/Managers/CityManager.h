// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CCity
{
public:
    string Name = {};
    wstring Description = {};

    CCity(const string &name, const wstring &description);
    CCity() = default;
    virtual ~CCity();
};

class CCityManager
{
public:
    deque<CCity> m_CityList;

    CCityManager();
    virtual ~CCityManager();

    void Init();
    void Clear();
    CCity GetCity(const string &name);
};

extern CCityManager g_CityManager;

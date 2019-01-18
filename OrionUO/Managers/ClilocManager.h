// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../BaseQueue.h"

typedef map<uint32_t, string> CLILOC_MAP;

class CCliloc : public CBaseQueueItem
{
public:
    string Language = "";
    bool Loaded = false;

private:
    // System (id < 1000000)
    CLILOC_MAP m_ClilocSystem;

    // Regular (id >= 1000000 && id < 3000000)
    CLILOC_MAP m_ClilocRegular;

    // Support (id >= 3000000)
    CLILOC_MAP m_ClilocSupport;

    string Load(uint32_t &id);
    wstring CamelCaseTest(bool toCamelCase, const string &result);
    wstring GetX(int id, bool toCamelCase, string &result);

public:
    CCliloc(const string &lang);
    virtual ~CCliloc();

    Wisp::CMappedFile m_File;

    string GetA(int id, bool toCamelCase = false, string result = {});  // FIXME
    wstring GetW(int id, bool toCamelCase = false, string result = {}); // FIXME
};

class CClilocManager : public CBaseQueue
{
private:
    CCliloc *m_LastCliloc{ nullptr };
    CCliloc *m_ENUCliloc{ nullptr };

public:
    CClilocManager();
    virtual ~CClilocManager();

    CCliloc *Cliloc(const string &lang);
    wstring ParseArgumentsToClilocString(int cliloc, bool toCamelCase, wstring args);
};

extern CClilocManager g_ClilocManager;

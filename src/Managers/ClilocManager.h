// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include <common/str.h>
#include <xuocore/mappedfile.h>
#include <map>
#include "../BaseQueue.h"

typedef std::map<uint32_t, astr_t> CLILOC_MAP;

class CCliloc : public CBaseQueueItem
{
public:
    astr_t Language = "";
    bool Loaded = false;

private:
    // System (id < 1000000)
    CLILOC_MAP m_ClilocSystem;

    // Regular (id >= 1000000 && id < 3000000)
    CLILOC_MAP m_ClilocRegular;

    // Support (id >= 3000000)
    CLILOC_MAP m_ClilocSupport;

    astr_t Load(uint32_t &id);
    wstr_t CamelCaseTest(bool toCamelCase, const astr_t &result);
    wstr_t GetX(int id, bool toCamelCase, astr_t &result);

public:
    CCliloc(const astr_t &lang);
    virtual ~CCliloc();

    CMappedFile m_File;

    astr_t GetA(int id, bool toCamelCase = false, astr_t result = {}); // FIXME
    wstr_t GetW(int id, bool toCamelCase = false, astr_t result = {}); // FIXME
};

class CClilocManager : public CBaseQueue
{
private:
    CCliloc *m_LastCliloc{ nullptr };
    CCliloc *m_ENUCliloc{ nullptr };
    wstr_t ParseArgumentsToCliloc(int cliloc, bool toCamelCase, wstr_t args);

public:
    CClilocManager();
    virtual ~CClilocManager();

    CCliloc *Cliloc(const astr_t &lang);
    wstr_t ParseArgumentsToClilocString(int cliloc, bool toCamelCase, wstr_t args);
    wstr_t ParseXmfHtmlArgumentsToCliloc(int cliloc, bool toCamelCase, wstr_t args);
};

extern CClilocManager g_ClilocManager;

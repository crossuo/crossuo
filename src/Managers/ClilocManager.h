// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include <common/str.h>
#include <xuocore/mappedfile.h>
#include <map>
#include "../BaseQueue.h"

class CCliloc : public CBaseQueueItem
{
    astr_t Load(uint32_t id);
    astr_t GetX(int id, bool toCamelCase, const char *default_value);

public:
    astr_t Language;
    bool Loaded = false;
    CMappedFile m_File;

    CCliloc(const astr_t &lang);
    virtual ~CCliloc();
    astr_t GetA(int id, bool toCamelCase = false, const char *default_value = nullptr);
    wstr_t GetW(int id, bool toCamelCase = false, const char *default_value = nullptr);
};

class CClilocManager : public CBaseQueue
{
    CCliloc *m_LastCliloc = nullptr;
    CCliloc *m_ENUCliloc = nullptr;
    wstr_t ParseArgumentsToCliloc(int cliloc, bool toCamelCase, wstr_t args);

public:
    CClilocManager() = default;
    virtual ~CClilocManager();
    CCliloc *Cliloc(const astr_t &lang);
    wstr_t ParseArgumentsToClilocString(int cliloc, bool toCamelCase, wstr_t args);
    wstr_t ParseXmfHtmlArgumentsToCliloc(int cliloc, bool toCamelCase, wstr_t args);
};

extern CClilocManager g_ClilocManager;

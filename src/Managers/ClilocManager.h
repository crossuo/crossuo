// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "../BaseQueue.h"
#include <xuocore/mappedfile.h>

typedef std::map<uint32_t, std::string> CLILOC_MAP;

class CCliloc : public CBaseQueueItem
{
public:
    std::string Language = "";
    bool Loaded = false;

private:
    // System (id < 1000000)
    CLILOC_MAP m_ClilocSystem;

    // Regular (id >= 1000000 && id < 3000000)
    CLILOC_MAP m_ClilocRegular;

    // Support (id >= 3000000)
    CLILOC_MAP m_ClilocSupport;

    std::string Load(uint32_t &id);
    std::wstring CamelCaseTest(bool toCamelCase, const std::string &result);
    std::wstring GetX(int id, bool toCamelCase, std::string &result);

public:
    CCliloc(const std::string &lang);
    virtual ~CCliloc();

    CMappedFile m_File;

    std::string GetA(int id, bool toCamelCase = false, std::string result = {});  // FIXME
    std::wstring GetW(int id, bool toCamelCase = false, std::string result = {}); // FIXME
};

class CClilocManager : public CBaseQueue
{
private:
    CCliloc *m_LastCliloc{ nullptr };
    CCliloc *m_ENUCliloc{ nullptr };
    std::wstring ParseArgumentsToCliloc(int cliloc, bool toCamelCase, std::wstring args);

public:
    CClilocManager();
    virtual ~CClilocManager();

    CCliloc *Cliloc(const std::string &lang);
    std::wstring ParseArgumentsToClilocString(int cliloc, bool toCamelCase, std::wstring args);
    std::wstring ParseXmfHtmlArgumentsToCliloc(int cliloc, bool toCamelCase, std::wstring args);
};

extern CClilocManager g_ClilocManager;

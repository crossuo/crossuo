// MIT License
// Copyright (C) October 2017 Hotride

#pragma once

#include "../BaseQueue.h"
#include <xuocore/mappedfile.h>

class CIntloc : public CBaseQueueItem
{
public:
    std::string Language = "";
    int FileIndex = -1;
    bool Loaded = false;

private:
    std::vector<std::wstring> m_Strings;

public:
    CIntloc(int fileIndex, const std::string &lang);
    virtual ~CIntloc();

    CMappedFile m_File;
    std::wstring Get(int id, bool toCamelCase = false);
};

class CIntlocManager : public CBaseQueue
{
private:
    CIntloc *Intloc(int fileIndex, const std::string &lang);

public:
    CIntlocManager();
    virtual ~CIntlocManager();

    std::wstring Intloc(const std::string &lang, uint32_t clilocID, bool isNewCliloc);
};

extern CIntlocManager g_IntlocManager;

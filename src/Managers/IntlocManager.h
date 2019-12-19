// MIT License
// Copyright (C) October 2017 Hotride

#pragma once

#include "../BaseQueue.h"
#include <xuocore/mappedfile.h>

class CIntloc : public CBaseQueueItem
{
public:
    astr_t Language = "";
    int FileIndex = -1;
    bool Loaded = false;

private:
    std::vector<wstr_t> m_Strings;

public:
    CIntloc(int fileIndex, const astr_t &lang);
    virtual ~CIntloc();

    CMappedFile m_File;
    wstr_t Get(int id, bool toCamelCase = false);
};

class CIntlocManager : public CBaseQueue
{
private:
    CIntloc *Intloc(int fileIndex, const astr_t &lang);

public:
    CIntlocManager();
    virtual ~CIntlocManager();

    wstr_t Intloc(const astr_t &lang, uint32_t clilocID, bool isNewCliloc);
};

extern CIntlocManager g_IntlocManager;

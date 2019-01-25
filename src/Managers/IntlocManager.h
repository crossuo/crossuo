// MIT License
// Copyright (C) October 2017 Hotride

#pragma once

#include "../BaseQueue.h"

class CIntloc : public CBaseQueueItem
{
public:
    string Language = "";
    int FileIndex = -1;
    bool Loaded = false;

private:
    vector<wstring> m_Strings;

public:
    CIntloc(int fileIndex, const string &lang);
    virtual ~CIntloc();

    Wisp::CMappedFile m_File;
    wstring Get(int id, bool toCamelCase = false);
};

class CIntlocManager : public CBaseQueue
{
private:
    CIntloc *Intloc(int fileIndex, const string &lang);

public:
    CIntlocManager();
    virtual ~CIntlocManager();

    wstring Intloc(const string &lang, uint32_t clilocID, bool isNewCliloc);
};

extern CIntlocManager g_IntlocManager;

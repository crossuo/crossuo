// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2017 Hotride

#pragma once

#include "../BaseQueue.h"
#include <xuocore/mappedfile.h>

class CIntloc : public CBaseQueueItem
{
    std::vector<wstr_t> m_Strings;

public:
    astr_t Language = "";
    int FileIndex = -1;
    bool Loaded = false;
    CMappedFile m_File;

    CIntloc(int fileIndex, const astr_t &lang);
    virtual ~CIntloc();
    wstr_t Get(int id, bool toCamelCase = false);
};

class CIntlocManager : public CBaseQueue
{
    CIntloc *Intloc(int fileIndex, const astr_t &lang);

public:
    CIntlocManager() = default;
    virtual ~CIntlocManager() = default;
    wstr_t Intloc(const astr_t &lang, uint32_t clilocID, bool isNewCliloc) const;
};

extern CIntlocManager g_IntlocManager;

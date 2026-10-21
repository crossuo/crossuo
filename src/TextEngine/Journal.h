// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "../BaseQueue.h"

struct CTextData;

class CJournal : public CBaseQueue
{
    int MaxSize = 0;
    int Size = 0;
    using CBaseQueue::Add;

public:
    CJournal(int maxSize);
    ~CJournal() override;

    void Add(CTextData *obj);
    virtual void Clear() override;
};

extern CJournal g_Journal;

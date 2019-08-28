// MIT License
// Copyright (C) August 2016 Hotride

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
    ~CJournal();

    void Add(CTextData *obj);
    virtual void Clear();
};

extern CJournal g_Journal;

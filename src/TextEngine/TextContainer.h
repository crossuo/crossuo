// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "../BaseQueue.h"

struct CTextData;

class CTextContainer : public CBaseQueue
{
    using CBaseQueue::Add;

public:
    int MaxSize = 0;
    int Size = 0;

    CTextContainer(int maxSize);
    ~CTextContainer();

    void Add(CTextData *obj);
    void Delete(CTextData *obj);
    virtual void Clear();
    void DrawSystemChat(int x, int y, int height);
};

extern CTextContainer g_SystemChat;

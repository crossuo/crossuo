// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#define QFOR(var, start, type) for (type var = (type)start; var != nullptr; var = (type)var->m_Next)
#define BQFOR(var, start) QFOR(var, start, CBaseQueueItem *)

class CBaseQueueItem;

class CBaseQueue
{
public:
    CBaseQueue();
    virtual ~CBaseQueue();

    CBaseQueueItem *m_Items;

    virtual void Clear();
    virtual CBaseQueueItem *Add(CBaseQueueItem *item);
    void Delete(CBaseQueueItem *item);
    void Delete(int index);
    CBaseQueueItem *Get(int index);
    int GetItemIndex(CBaseQueueItem *item);
    int GetItemsCount();
    void Unlink(CBaseQueueItem *item);
    void Insert(CBaseQueueItem *first, CBaseQueueItem *item);
    virtual void MoveToFront(CBaseQueueItem *item);
    void MoveToBack(CBaseQueueItem *item);
    bool Move(CBaseQueueItem *item, bool up);
    bool Empty() const { return (m_Items == nullptr); }
    CBaseQueueItem *Last();
};

class CBaseQueueItem : public CBaseQueue
{
public:
    CBaseQueueItem();
    virtual ~CBaseQueueItem();

    CBaseQueueItem *m_Next;
    CBaseQueueItem *m_Prev;
};

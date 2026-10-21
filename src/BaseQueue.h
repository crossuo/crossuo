// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once
#pragma once

class CBaseQueueItem;

class CBaseQueue
{
public:
    CBaseQueueItem *m_Items = nullptr;
    virtual ~CBaseQueue();

    virtual void Clear();
    virtual CBaseQueueItem *Add(CBaseQueueItem *item);
    void Delete(CBaseQueueItem *item);
    void Delete(int index);
    CBaseQueueItem *Get(int index) const;
    int GetItemIndex(CBaseQueueItem *item) const;
    int GetItemsCount() const;
    void Unlink(CBaseQueueItem *item);
    void Insert(CBaseQueueItem *first, CBaseQueueItem *item);
    virtual void MoveToFront(CBaseQueueItem *item);
    void MoveToBack(CBaseQueueItem *item);
    bool Move(CBaseQueueItem *item, bool up);
    bool Empty() const { return (m_Items == nullptr); }
    CBaseQueueItem *Last() const;
};

class CBaseQueueItem : public CBaseQueue
{
public:
    virtual ~CBaseQueueItem() override;
    CBaseQueueItem *m_Next = nullptr;
    CBaseQueueItem *m_Prev = nullptr;
};

// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (basequeue.h) - intrusive doubly-linked list, Qt-free.
// QFOR/BQFOR are guarded because the client's src/BaseQueue.h defines the same
// (identical) macros; identical redefinitions are legal either way.

#ifndef XUOASSIST_BASEQUEUE_H
#define XUOASSIST_BASEQUEUE_H

namespace xuoassist {

#ifndef QFOR
#define QFOR(var, start, type) for (type var = (type)start; var != nullptr; var = (type)var->m_Next)
#endif
#ifndef BQFOR
#define BQFOR(var, start) QFOR(var, start, CBaseQueueItem *)
#endif

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
    void Delete(const int &index);
    CBaseQueueItem *Get(int index);
    int GetItemIndex(CBaseQueueItem *item);
    int GetItemsCount();
    void Unlink(CBaseQueueItem *item);
    void Insert(CBaseQueueItem *first, CBaseQueueItem *item);
    virtual void MoveToFront(CBaseQueueItem *item);
    void MoveToBack(CBaseQueueItem *item);
    bool Move(CBaseQueueItem *item, const bool &up);
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

} // namespace xuoassist

#endif // XUOASSIST_BASEQUEUE_H

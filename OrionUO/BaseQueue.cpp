// MIT License
// Copyright (C) August 2016 Hotride

#include "BaseQueue.h"

CBaseQueueItem::CBaseQueueItem()
    : m_Next(nullptr)
    , m_Prev(nullptr)
{
}

CBaseQueueItem::~CBaseQueueItem()
{
    DEBUG_TRACE_FUNCTION;
    Clear();

    CBaseQueueItem *item = m_Next;
    while (item != nullptr && item != this)
    {
        CBaseQueueItem *next = item->m_Next;
        item->m_Next = nullptr;
        delete item;
        item = next;
    }
}

CBaseQueue::CBaseQueue()
    : m_Items(nullptr)
{
}

CBaseQueue::~CBaseQueue()
{
    DEBUG_TRACE_FUNCTION;
    Clear();
}

void CBaseQueue::Clear()
{
    DEBUG_TRACE_FUNCTION;
    if (m_Items != nullptr)
    {
        CBaseQueueItem *item = m_Items;
        m_Items = nullptr;
        while (item != nullptr)
        {
            CBaseQueueItem *next = item->m_Next;
            item->m_Next = nullptr;
            delete item;
            item = next;
        }
    }
}

CBaseQueueItem *CBaseQueue::Add(CBaseQueueItem *item)
{
    DEBUG_TRACE_FUNCTION;
    if (item != nullptr)
    {
        if (m_Items == nullptr)
        {
            m_Items = item;
        }
        else
        {
            CBaseQueueItem *current = m_Items;
            while (current->m_Next != nullptr)
            {
                current = current->m_Next;
            }
            current->m_Next = item;
            item->m_Prev = current;
        }
    }
    return item;
}

void CBaseQueue::Delete(CBaseQueueItem *item)
{
    DEBUG_TRACE_FUNCTION;
    if (item != nullptr)
    {
        Unlink(item);
        item->m_Next = nullptr;
        item->m_Prev = nullptr;
        delete item;
    }
}

void CBaseQueue::Delete(int index)
{
    DEBUG_TRACE_FUNCTION;
    Delete(Get(index));
}

int CBaseQueue::GetItemIndex(CBaseQueueItem *item)
{
    DEBUG_TRACE_FUNCTION;
    int index = 0;
    BQFOR (current, m_Items)
    {
        if (current == item)
        {
            return index;
        }
        index++;
    }
    return -1;
}

int CBaseQueue::GetItemsCount()
{
    DEBUG_TRACE_FUNCTION;
    int count = 0;
    BQFOR (current, m_Items)
    {
        count++;
    }
    return count;
}

CBaseQueueItem *CBaseQueue::Get(int index)
{
    DEBUG_TRACE_FUNCTION;
    CBaseQueueItem *item = m_Items;
    for (; item != nullptr && (index != 0); item = item->m_Next, index--)
    {
        ;
    }
    return item;
}

void CBaseQueue::Insert(CBaseQueueItem *first, CBaseQueueItem *item)
{
    DEBUG_TRACE_FUNCTION;
    if (first == nullptr)
    {
        item->m_Next = m_Items;
        item->m_Prev = nullptr;
        if (m_Items != nullptr)
        {
            m_Items->m_Prev = item;
        }
        m_Items = item;
    }
    else
    {
        CBaseQueueItem *next = first->m_Next;
        item->m_Next = next;
        item->m_Prev = first;
        first->m_Next = item;
        if (next != nullptr)
        {
            next->m_Prev = item;
        }
    }
}

void CBaseQueue::Unlink(CBaseQueueItem *item)
{
    DEBUG_TRACE_FUNCTION;
    if (item != nullptr)
    {
        if (item == m_Items)
        {
            m_Items = m_Items->m_Next;
            if (m_Items != nullptr)
            {
                m_Items->m_Prev = nullptr;
            }
        }
        else
        {
            item->m_Prev->m_Next = item->m_Next;
            if (item->m_Next != nullptr)
            {
                item->m_Next->m_Prev = item->m_Prev;
            }
        }
    }
}

void CBaseQueue::MoveToFront(CBaseQueueItem *item)
{
    DEBUG_TRACE_FUNCTION;
    if (item != nullptr && item != m_Items)
    {
        Unlink(item);
        if (m_Items != nullptr)
        {
            m_Items->m_Prev = item;
        }

        item->m_Next = m_Items;
        item->m_Prev = nullptr;
        m_Items = item;
    }
}

void CBaseQueue::MoveToBack(CBaseQueueItem *item)
{
    DEBUG_TRACE_FUNCTION;
    if (item != nullptr)
    {
        Unlink(item);
        CBaseQueueItem *last = Last();
        if (last == nullptr)
        {
            m_Items = item;
        }
        else
        {
            last->m_Next = item;
        }
        item->m_Prev = last;
        item->m_Next = nullptr;
    }
}

bool CBaseQueue::Move(CBaseQueueItem *item, bool up)
{
    DEBUG_TRACE_FUNCTION;
    bool result = (item != nullptr);
    if (result)
    {
        if (up)
        {
            CBaseQueueItem *prev = item->m_Prev;
            result = (prev != nullptr);
            if (result)
            {
                if (prev == m_Items)
                {
                    prev->m_Prev = item;
                    prev->m_Next = item->m_Next;
                    m_Items = item;
                    item->m_Prev = nullptr;
                    item->m_Next = prev;
                }
                else
                {
                    CBaseQueueItem *prevprev = prev->m_Prev;
                    prev->m_Prev = item;
                    prev->m_Next = item->m_Next;
                    prevprev->m_Next = item;
                    item->m_Prev = prevprev;
                    item->m_Next = prev;
                }
            }
        }
        else
        {
            CBaseQueueItem *next = item->m_Next;
            result = (next != nullptr);
            if (result)
            {
                if (item == m_Items)
                {
                    item->m_Next = next->m_Next;
                    item->m_Prev = next;
                    m_Items = item;
                    m_Items->m_Prev = nullptr;
                    m_Items->m_Next = item;
                }
                else
                {
                    CBaseQueueItem *prev = item->m_Prev;
                    prev->m_Next = next;
                    item->m_Next = next->m_Next;
                    item->m_Prev = next;
                    next->m_Prev = prev;
                    next->m_Next = item;
                }
            }
        }
    }
    return result;
}

CBaseQueueItem *CBaseQueue::Last()
{
    DEBUG_TRACE_FUNCTION;
    CBaseQueueItem *last = m_Items;
    while (last != nullptr && last->m_Next != nullptr)
    {
        last = last->m_Next;
    }
    return last;
}

// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// Ported from X:UO Assist (gameobjects/gameobject.cpp). The version gate is a
// shim (core/textconv.h partner, see GetClientVersion()) until the packet
// manager round ports g_PacketManager; max -> std::max, lists -> std::vector.

#include "gameobject.h"
#include "gameworld.h"
#include "gameplayer.h"
#include "core/textconv.h"
#include "core/clientversion.h"
#include <algorithm>
#include <cstdlib>

namespace xuoassist {

CGameObject::CGameObject(const uint &serial)
    : m_Serial(serial)
{
}

CGameObject::~CGameObject()
{
    m_Next = nullptr;
    m_Prev = nullptr;

    Clear();
}

void CGameObject::OnChangeFlags(const uchar &newFlags)
{
    if (IsPlayer())
    {
        if ((m_Flags & 0x80) != (newFlags & 0x80))
        {
            g_Player->SetStealthSteps(0);
        }
    }
}

void CGameObject::Clear()
{
    if (!Empty())
    {
        CGameObject *obj = (CGameObject *)m_Items;

        while (obj != nullptr)
        {
            CGameObject *next = (CGameObject *)obj->m_Next;

            g_World->RemoveObject(obj);

            obj = next;
        }

        m_Items = nullptr;
    }
}

void CGameObject::ClearUnequipped()
{
    if (!Empty())
    {
        CGameObject *newFirstItem = nullptr;
        CGameObject *obj = (CGameObject *)m_Items;

        while (obj != nullptr)
        {
            CGameObject *next = (CGameObject *)obj->m_Next;

            if (((CGameItem *)obj)->GetLayer() != OL_NONE)
            {
                if (newFirstItem == nullptr)
                {
                    newFirstItem = obj;
                }
            }
            else
            {
                g_World->RemoveObject(obj);
            }

            obj = next;
        }

        m_Items = newFirstItem;
    }
}

bool CGameObject::Poisoned()
{
    if (GetClientVersion() >= CV_7000)
    {
        return m_SA_Poisoned;
    }
    {
        return (m_Flags & 0x04) != 0;
    }
}

bool CGameObject::Flying()
{
    if (GetClientVersion() >= CV_7000)
    {
        return (m_Flags & 0x04) != 0;
    }
    {
        return false;
    }
}

void CGameObject::AddObject(CGameObject *obj)
{
    g_World->RemoveFromContainer(obj);

    if (m_Next == nullptr)
    {
        m_Next = obj;
        m_Next->m_Prev = this;
        m_Next->m_Next = nullptr;

        ((CGameObject *)m_Next)->SetContainer(m_Container);
    }
    else
    {
        CGameObject *item = (CGameObject *)m_Next;

        while (item->m_Next != nullptr)
        {
            item = (CGameObject *)item->m_Next;
        }

        item->m_Next = obj;
        obj->m_Next = nullptr;
        obj->m_Prev = item;

        obj->SetContainer(m_Container);
    }
}

void CGameObject::AddItem(CGameObject *obj)
{
    if (obj->GetContainer() != 0xFFFFFFFF)
    {
        return;
    }

    g_World->RemoveFromContainer(obj);

    if (m_Items != nullptr)
    {
        CGameObject *item = (CGameObject *)Last();

        item->m_Next = obj;
        obj->m_Next = nullptr;
        obj->m_Prev = item;
    }
    else
    {
        m_Items = obj;
        m_Items->m_Next = nullptr;
        m_Items->m_Prev = nullptr;
    }

    obj->SetContainer(m_Serial);
}

void CGameObject::Reject(CGameObject *obj)
{
    if (obj->GetContainer() != m_Serial)
    {
        return;
    }

    if (m_Items != nullptr)
    {
        if (((CGameObject *)m_Items)->GetSerial() == obj->GetSerial())
        {
            if (m_Items->m_Next != nullptr)
            {
                m_Items = m_Items->m_Next;
                m_Items->m_Prev = nullptr;
            }
            else
            {
                m_Items = nullptr;
            }
        }
        else
        {
            if (obj->m_Next != nullptr)
            {
                if (obj->m_Prev != nullptr)
                {
                    obj->m_Prev->m_Next = obj->m_Next;
                    obj->m_Next->m_Prev = obj->m_Prev;
                }
                else
                { // WTF???
                    obj->m_Next->m_Prev = nullptr;
                }
            }
            else if (obj->m_Prev != nullptr)
            {
                obj->m_Prev->m_Next = nullptr;
            }
        }
    }

    obj->m_Next = nullptr;
    obj->m_Prev = nullptr;
    obj->SetContainer(0xFFFFFFFF);
}

CGameObject *CGameObject::GetTopObject()
{
    CGameObject *obj = this;

    while (obj->GetContainer() != 0xFFFFFFFF)
    {
        obj = g_World->FindWorldObject(obj->GetContainer());
    }

    return obj;
}

CGameItem *CGameObject::FindLayer(const int &layer)
{
    QFOR(obj, m_Items, CGameItem *)
    {
        if (obj->GetLayer() == layer)
        {
            return obj;
        }
    }

    return nullptr;
}

int CGameObject::CountItems(const CFindItem &findListItem, const bool &recurse)
{
    int count = 0;

    QFOR(item, m_Items, CGameItem *)
    {
        if (findListItem.Found(item->GetGraphic(), item->GetColor()))
        {
            count += item->GetCount();
        }

        if (recurse && !item->Empty() && item->GetLayer() != OL_BANK)
        {
            count += item->CountItems(findListItem, recurse);
        }
    }

    return count;
}

int CGameObject::CountItems(const ushort &graphic, const ushort &color, const bool &recurse)
{
    int count = 0;

    QFOR(item, m_Items, CGameItem *)
    {
        if ((graphic == 0xFFFF || graphic == item->GetGraphic()) &&
            (color == 0xFFFF || color == item->GetColor()))
        {
            count += item->GetCount();
        }

        if (recurse && !item->Empty() && item->GetLayer() != OL_BANK)
        {
            count += item->CountItems(graphic, color, recurse);
        }
    }

    return count;
}

CGameObject *CGameObject::FindType(
    const CFindItem &findListItem,
    const vector<CIgnoreItem> &ignoreList,
    const uchar &flags,
    const bool &recurseSearch,
    vector<string> &list)
{
    CGameObject *result = nullptr;

    vector<CGameObject *> containers;

    QFOR(item, m_Items, CGameItem *)
    {
        if (!item->GetIgnored() && findListItem.Found(item->GetGraphic(), item->GetColor()))
        {
            bool ignored = false;

            if (!ignoreList.empty())
            {
                for (const CIgnoreItem &ii : ignoreList)
                {
                    if (ii.Found(item->GetSerial(), item->GetGraphic(), item->GetColor()))
                    {
                        ignored = true;
                        break;
                    }
                }
            }

            if (!ignored)
            {
                list.push_back(SerialToText(item->GetSerial()));

                result = item;

                if ((flags & FTF_FAST) != 0)
                {
                    break;
                }
            }
        }

        if (recurseSearch && !item->Empty() && item->GetLayer() != OL_BANK)
        {
            containers.push_back(item);
        }
    }

    if (((flags & FTF_FAST) == 0) || result == nullptr)
    {
        for (CGameObject *item : containers)
        {
            CGameObject *temp =
                item->FindType(findListItem, ignoreList, flags, recurseSearch, list);

            if (temp != nullptr)
            {
                result = temp;

                if ((flags & FTF_FAST) != 0)
                {
                    break;
                }
            }
        }
    }

    return result;
}

int GetDistance(CGameObject *obj1, CGameObject *obj2)
{
    if (obj1 != nullptr && obj2 != nullptr)
    {
        int distx = abs(obj2->GetX() - obj1->GetX());
        int disty = abs(obj2->GetY() - obj1->GetY());

        return std::max(distx, disty);
    }

    return 100500;
}

int GetDistance(const int &x1, const int &y1, const int &x2, const int &y2)
{
    int distx = abs(x2 - x1);
    int disty = abs(y2 - y1);

    return std::max(distx, disty);
}

} // namespace xuoassist

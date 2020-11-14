// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "SkillGroup.h"
#include "Managers/SkillsManager.h"

CSkillGroupObject::CSkillGroupObject()
{
    memset(m_Items, 0xFF, sizeof(m_Items));
}

CSkillGroupObject::~CSkillGroupObject()
{
}

uint8_t CSkillGroupObject::GetItem(intptr_t index)
{
    if (index < 0 || index >= Count)
    {
        return 0xFF;
    }

    return m_Items[index];
}

void CSkillGroupObject::Add(uint8_t index)
{
    if (Contains(index))
    {
        return;
    }

    m_Items[Count] = index;
    Count++;
}

void CSkillGroupObject::AddSorted(uint8_t index)
{
    if (Contains(index))
    {
        return;
    }

    m_Items[Count] = index;
    Count++;

    Sort();
}

void CSkillGroupObject::Remove(uint8_t index)
{
    bool removed = false;

    for (int i = 0; i < Count; i++)
    {
        if (m_Items[i] == index)
        {
            removed = true;

            for (; i < Count - 1; i++)
            {
                m_Items[i] = m_Items[i + 1];
            }

            break;
        }
    }

    if (removed)
    {
        Count--;

        if (Count < 0)
        {
            Count = 0;
        }

        m_Items[Count] = 0xFF;
    }
}

bool CSkillGroupObject::Contains(uint8_t index)
{
    for (int i = 0; i < Count; i++)
    {
        if (m_Items[i] == index)
        {
            return true;
        }
    }

    return false;
}

void CSkillGroupObject::Sort()
{
    uint8_t table[60] = { 0 };
    int Ptr = 0;

    for (uint32_t i = 0; i < g_SkillsManager.Count; i++)
    {
        for (int j = 0; j < Count; j++)
        {
            if (g_SkillsManager.GetSortedIndex((int)i) == m_Items[j])
            {
                table[Ptr] = m_Items[j];
                Ptr++;
                break;
            }
        }
    }

    for (int j = 0; j < Count; j++)
    {
        m_Items[j] = table[j];
    }
}

void CSkillGroupObject::TransferTo(CSkillGroupObject *group)
{
    for (int i = 0; i < Count; i++)
    {
        group->Add(m_Items[i]);
    }

    group->Sort();
}

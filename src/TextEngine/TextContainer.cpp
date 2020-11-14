// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "TextContainer.h"
#include "TextData.h"
#include "Globals.h" // g_Ticks

CTextContainer g_SystemChat(30);

CTextContainer::CTextContainer(int maxSize)
    : MaxSize(maxSize)
{
}

CTextContainer::~CTextContainer()
{
}

void CTextContainer::Clear()
{
    CBaseQueue::Clear();

    Size = 0;
}

void CTextContainer::Add(CTextData *obj)
{
    CBaseQueue::Add(obj);

    if (Size >= MaxSize)
    {
        CBaseQueue::Delete(m_Items);
    }
    else
    {
        Size++;
    }
}

void CTextContainer::Delete(CTextData *obj)
{
    CBaseQueue::Delete(obj);

    Size--;
}

void CTextContainer::DrawSystemChat(int x, int y, int height)
{
    int offset = (y + height) - 41;

    CTextData *td = (CTextData *)g_SystemChat.Last();

    while (td != nullptr && offset >= y)
    {
        auto &tth = td->m_TextSprite;

        offset -= tth.Height;

        if (td->Timer >= g_Ticks)
        {
            tth.Draw(x, offset);
        }

        td = (CTextData *)td->m_Prev;
    }
}

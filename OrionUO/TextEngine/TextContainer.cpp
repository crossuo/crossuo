// MIT License
// Copyright (C) August 2016 Hotride

#include "TextContainer.h"
#include "TextEngine/TextData.h"

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
    DEBUG_TRACE_FUNCTION;
    CBaseQueue::Clear();

    Size = 0;
}

void CTextContainer::Add(CTextData *obj)
{
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
    CBaseQueue::Delete(obj);

    Size--;
}

void CTextContainer::DrawSystemChat(int x, int y, int height)
{
    DEBUG_TRACE_FUNCTION;
    int offset = (y + height) - 41;

    CTextData *td = (CTextData *)g_SystemChat.Last();

    while (td != nullptr && offset >= y)
    {
        CGLTextTexture &tth = td->m_Texture;

        offset -= tth.Height;

        if (td->Timer >= g_Ticks)
        {
            tth.Draw(x, offset);
        }

        td = (CTextData *)td->m_Prev;
    }
}

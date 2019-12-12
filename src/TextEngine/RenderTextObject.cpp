// MIT License
// Copyright (C) August 2016 Hotride

#include "RenderTextObject.h"
#include "TextRenderer.h"
#include "../SelectedObject.h"

CRenderTextObject::CRenderTextObject()
    : CRenderObject(0, 0, 0, 0, 0)
{
}

CRenderTextObject::~CRenderTextObject()
{
    UnlinkDraw();
}

void CRenderTextObject::UnlinkDraw()
{
    if (m_NextDraw != nullptr)
    {
        m_NextDraw->m_PrevDraw = m_PrevDraw;
    }

    if (m_PrevDraw != nullptr)
    {
        m_PrevDraw->m_NextDraw = m_NextDraw;
    }

    m_NextDraw = nullptr;
    m_PrevDraw = nullptr;
}

void CRenderTextObject::ToTop()
{
    CRenderTextObject *obj = this;

    while (obj != nullptr)
    {
        if (obj->m_PrevDraw == nullptr)
        {
            break;
        }

        obj = obj->m_PrevDraw;
    }

    CTextRenderer *tr = (CTextRenderer *)obj;
    tr->ToTop(this);
}

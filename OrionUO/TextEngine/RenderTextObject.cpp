// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** RenderTextObject.cpp
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#include "stdafx.h"

CRenderTextObject::CRenderTextObject()
    : CRenderObject(0, 0, 0, 0, 0)
{
}

CRenderTextObject::~CRenderTextObject()
{
    DEBUG_TRACE_FUNCTION;
    UnlinkDraw();
}

void CRenderTextObject::UnlinkDraw()
{
    DEBUG_TRACE_FUNCTION;
    if (m_NextDraw != nullptr)
        m_NextDraw->m_PrevDraw = m_PrevDraw;

    if (m_PrevDraw != nullptr)
        m_PrevDraw->m_NextDraw = m_NextDraw;

    m_NextDraw = nullptr;
    m_PrevDraw = nullptr;
}

void CRenderTextObject::ToTop()
{
    DEBUG_TRACE_FUNCTION;
    CRenderTextObject *obj = this;

    while (obj != nullptr)
    {
        if (obj->m_PrevDraw == nullptr)
            break;

        obj = obj->m_PrevDraw;
    }

    CTextRenderer *tr = (CTextRenderer *)obj;
    tr->ToTop(this);
}


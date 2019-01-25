// MIT License
// Copyright (C) August 2016 Hotride

#include "SelectedObject.h"
#include "RenderObject.h"

CSelectedObject g_SelectedObject;
CSelectedObject g_LastSelectedObject;

CSelectedObject::CSelectedObject()
{
}

CSelectedObject::~CSelectedObject()
{
}

void CSelectedObject::Clear()
{
    DEBUG_TRACE_FUNCTION;
    Object = nullptr;
    Gump = nullptr;
    Serial = 0;
}

void CSelectedObject::Clear(CRenderObject *obj)
{
    DEBUG_TRACE_FUNCTION;
    if (obj == Object)
    {
        Object = nullptr;
        Gump = nullptr;
        Serial = 0;
    }
}

void CSelectedObject::Init(CRenderObject *obj, CGump *gump)
{
    DEBUG_TRACE_FUNCTION;
    Object = obj;
    Gump = gump;
    Serial = (obj != nullptr ? obj->Serial : 0);
}

void CSelectedObject::Init(const CSelectedObject &obj)
{
    DEBUG_TRACE_FUNCTION;
    Object = obj.Object;
    Gump = obj.Gump;
    Serial = (obj.Object != nullptr ? obj.Object->Serial : 0);
}

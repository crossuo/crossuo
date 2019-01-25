// MIT License
// Copyright (C) August 2016 Hotride

#include "RenderObject.h"
#include "PressedObject.h"
#include "SelectedObject.h"
#include "ClickObject.h"

CRenderObject::CRenderObject(int serial, uint16_t graphic, uint16_t color, short x, short y)
    : Serial(serial)
    , Graphic(graphic)
    , Color(color)
    , m_X(x)
    , m_Y(y)
{
    UpdateRealDrawCoordinates();
}

CRenderObject::~CRenderObject()
{
    DEBUG_TRACE_FUNCTION;
    g_ClickObject.Clear(this);
    g_SelectedObject.Clear(this);
    g_LastSelectedObject.Clear(this);
    g_PressedObject.Clear(this);
}

void CRenderObject::SetX(int val)
{
    m_X = val;
    UpdateRealDrawCoordinates();
}

void CRenderObject::SetY(int val)
{
    m_Y = val;
    UpdateRealDrawCoordinates();
}

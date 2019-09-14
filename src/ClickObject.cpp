// MIT License
// Copyright (C) August 2016 Hotride

#include "ClickObject.h"
#include "SelectedObject.h"

CClickObject g_ClickObject;

void CClickObject::Clear(CRenderObject *obj)
{
    if (obj == Object)
    {
        Object = nullptr;
        Gump = nullptr;
        Timer = 0;
        X = 0;
        Y = 0;
        Enabled = false;
        Page = 0;
    }
}

void CClickObject::Clear()
{
    Object = nullptr;
    Gump = nullptr;
    Timer = 0;
    //m_X = 0;
    //m_Y = 0;
    Enabled = false;
    Page = 0;
}

void CClickObject::Init(CRenderObject *obj, CGump *gump)
{
    Enabled = true;
    Object = obj;
    Gump = gump;
    Timer = 0;
    X = 0;
    Y = 0;
    Page = 0;
}

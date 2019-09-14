// MIT License
// Copyright (C) August 2016 Hotride

#include "PressedObject.h"
#include "SelectedObject.h"
#include "GUI/BaseGUI.h"

CPressedObject g_PressedObject;

CPressedObject::CPressedObject()
{
}

CPressedObject::~CPressedObject()
{
}

void CPressedObject::Init(const CPressedObject &obj)
{
    LeftObject = obj.LeftObject;
    LeftGump = obj.LeftGump;
    LeftSerial = obj.LeftSerial;

    RightObject = obj.RightObject;
    RightGump = obj.RightGump;
    RightSerial = obj.RightSerial;

    MidObject = obj.MidObject;
    MidGump = obj.MidGump;
    MidSerial = obj.MidSerial;
}

void CPressedObject::ClearAll()
{
    ClearLeft();
    ClearRight();
    ClearMid();
}

void CPressedObject::Clear(CRenderObject *obj)
{
    if (obj == LeftObject)
    {
        LeftObject = nullptr;
        LeftSerial = 0;
    }

    if (obj == RightObject)
    {
        RightObject = nullptr;
        RightSerial = 0;
    }

    if (obj == MidObject)
    {
        MidObject = nullptr;
        MidSerial = 0;
    }
}

void CPressedObject::ClearLeft()
{
    LeftObject = nullptr;
    LeftGump = nullptr;
    LeftSerial = 0;
}

void CPressedObject::InitLeft(CRenderObject *obj, CGump *gump)
{
    LeftObject = obj;
    LeftGump = gump;
    LeftSerial = (obj != nullptr ? obj->Serial : 0);
}

void CPressedObject::InitLeft(const CSelectedObject &obj)
{
    LeftObject = obj.Object;
    LeftGump = obj.Gump;
    LeftSerial = (LeftObject != nullptr ? obj.Serial : 0);
}

void CPressedObject::ClearRight()
{
    RightObject = nullptr;
    RightGump = nullptr;
    RightSerial = 0;
}

void CPressedObject::InitRight(CRenderObject *obj, CGump *gump)
{
    RightObject = obj;
    RightGump = gump;
    RightSerial = (obj != nullptr ? obj->Serial : 0);
}

void CPressedObject::InitRight(const CSelectedObject &obj)
{
    RightObject = obj.Object;
    RightGump = obj.Gump;
    RightSerial = (RightObject != nullptr ? obj.Serial : 0);
}

void CPressedObject::ClearMid()
{
    MidObject = nullptr;
    MidGump = nullptr;
    MidSerial = 0;
}

void CPressedObject::InitMid(CRenderObject *obj, CGump *gump)
{
    MidObject = obj;
    MidGump = gump;
    MidSerial = (obj != nullptr ? obj->Serial : 0);
}

void CPressedObject::InitMid(const CSelectedObject &obj)
{
    MidObject = obj.Object;
    MidGump = obj.Gump;
    MidSerial = (MidObject != nullptr ? obj.Serial : 0);
}

bool CPressedObject::TestMoveOnDrag()
{
    return (LeftObject != nullptr && LeftObject->IsGUI() && ((CBaseGUI *)LeftObject)->MoveOnDrag);
}

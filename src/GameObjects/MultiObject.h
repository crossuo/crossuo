// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "RenderStaticObject.h"

class CMultiObject : public CRenderStaticObject
{
public:
    bool OnTarget = false;
    uint16_t OriginalGraphic = 0;
    int State = 0;

    CMultiObject(uint16_t graphic, short x, short y, char z, int flags);
    virtual ~CMultiObject();
    virtual void UpdateGraphicBySeason();
    virtual void Draw(int x, int y);
    virtual void Select(int x, int y);
    bool IsMultiObject() { return true; }
    virtual bool IsCustomHouseMulti() { return false; }
};

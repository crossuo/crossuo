// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include <xuocore/enumlist.h>
#include "../RenderWorldObject.h"

class CMapObject : public CRenderWorldObject
{
public:
    CMapObject(
        RENDER_OBJECT_TYPE type,
        int serial,
        uint16_t graphic,
        uint16_t color,
        short x,
        short y,
        char z);

    virtual ~CMapObject();
};

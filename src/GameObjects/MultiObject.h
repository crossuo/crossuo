// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "RenderStaticObject.h"

class CMultiObject : public CRenderStaticObject
{
public:
    bool OnTarget = false;
    uint16_t OriginalGraphic = 0;
    int State = 0;

    CMultiObject(uint16_t graphic, short x, short y, char z, int flags);
    virtual ~CMultiObject() override;
    virtual void UpdateGraphicBySeason() override;
    virtual void Draw(int x, int y) override;
    virtual void Select(int x, int y) override;
    bool IsMultiObject() override { return true; }
    virtual bool IsCustomHouseMulti() { return false; }
};

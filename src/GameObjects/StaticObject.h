// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "RenderStaticObject.h"

class CStaticObject : public CRenderStaticObject
{
    // Original picture index
    uint16_t OriginalGraphic = 0;

public:
    CStaticObject(int serial, uint16_t graphic, uint16_t color, short x, short y, char z);
    virtual ~CStaticObject() override = default;

    virtual void UpdateGraphicBySeason() override;

    // Draw the object
    virtual void Draw(int x, int y) override;

    // Select the object
    virtual void Select(int x, int y) override;

    // This is a static object
    bool IsStaticObject() override { return true; }
};

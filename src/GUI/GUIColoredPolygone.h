// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "GUIPolygonal.h"

class CGUIColoredPolygone : public CGUIPolygonal
{
public:
    //! R color component
    uint8_t ColorR = 0;

    //! G color component
    uint8_t ColorG = 0;

    //! B color component
    uint8_t ColorB = 0;

    //! Alpha value
    uint8_t ColorA = 0;

    //! Whether to draw the white dot
    bool DrawDot = false;

    //! Selection flag of the current component
    bool Focused = false;

    CGUIColoredPolygone(
        int serial, uint16_t color, int x, int y, int width, int height, int polygoneColor);
    virtual ~CGUIColoredPolygone();

    //! Update the color
    void UpdateColor(uint16_t color, int polygoneColor);

    virtual void Draw(bool checktrans = false);

    virtual void OnMouseEnter();
    virtual void OnMouseExit();
};

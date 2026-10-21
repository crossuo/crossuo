// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include "GUIPolygonal.h"

class CGUIHitBox : public CGUIPolygonal
{
public:
    //! Page index to move to (when used, functionally like CGUIButton)
    int ToPage = -1;

    CGUIHitBox(int serial, int x, int y, int width, int height, bool callOnMouseUp = false);
    virtual ~CGUIHitBox() override;

    virtual void OnMouseEnter() override;
    virtual void OnMouseExit() override;
};

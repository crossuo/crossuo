// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUIPolygonal.h"

class CGUIHitBox : public CGUIPolygonal
{
public:
    //!Индекс страницы для перемещения (если используется, по фукнционалу CGUIButton)
    int ToPage = -1;

    CGUIHitBox(int serial, int x, int y, int width, int height, bool callOnMouseUp = false);
    virtual ~CGUIHitBox();

    virtual void OnMouseEnter();
    virtual void OnMouseExit();
};

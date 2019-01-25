// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUIPolygonal.h"

class CGUIScissor : public CGUIPolygonal
{
    //!Координата компоненты по оси X контейнера, в котором находится элемент, относительно начала гампа
    int BaseX = 0;

    //!Координата компоненты по оси Y контейнера, в котором находится элемент, относительно начала гампа
    int BaseY = 0;

public:
    //!Гамп-родитель
    CGump *GumpParent = nullptr;

    CGUIScissor(
        bool enabled,
        int baseX = 0,
        int baseY = 0,
        int x = 0,
        int y = 0,
        int width = 0,
        int height = 0);
    virtual ~CGUIScissor();

    virtual void Draw(bool checktrans = false);
};

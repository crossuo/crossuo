// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUIPolygonal.h"

class CGUIColoredPolygone : public CGUIPolygonal
{
public:
    //!R-компонента цвета
    uint8_t ColorR = 0;

    //!G-компонента цвета
    uint8_t ColorG = 0;

    //!B-компонента цвета
    uint8_t ColorB = 0;

    //!Значение альфа-канала
    uint8_t ColorA = 0;

    //!Отрисовывать ли белую точку
    bool DrawDot = false;

    //!Флаг выбора текущей компоненты
    bool Focused = false;

    CGUIColoredPolygone(
        int serial, uint16_t color, int x, int y, int width, int height, int polygoneColor);
    virtual ~CGUIColoredPolygone();

    //!Обновить цвет
    void UpdateColor(uint16_t color, int polygoneColor);

    virtual void Draw(bool checktrans = false);

    virtual void OnMouseEnter();
    virtual void OnMouseExit();
};

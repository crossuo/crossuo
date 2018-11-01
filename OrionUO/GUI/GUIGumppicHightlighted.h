/***********************************************************************************
**
** GUIGumppicHightlighted.h
**
** Компонента для отображения гампа с возможностью подсветки (для шахматных фигур)
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef GUIGUMPPICHIGHTLIGHTED_H
#define GUIGUMPPICHIGHTLIGHTED_H

class CGUIGumppicHightlighted : public CGUIDrawObject
{
    //!Цвет подсветки
    uint16_t SelectedColor = 0;

public:
    CGUIGumppicHightlighted(
        int serial, uint16_t graphic, uint16_t color, uint16_t electedColor, int x, int y);
    virtual ~CGUIGumppicHightlighted();

    virtual void SetShaderMode();

    virtual void OnMouseEnter();
    virtual void OnMouseExit();
};

#endif

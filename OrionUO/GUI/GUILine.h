/***********************************************************************************
**
** GUILine.h
**
** Компонента для отображения линии
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef GUILINE_H
#define GUILINE_H

class CGUILine : public CBaseGUI
{
    //!Целевая координата по оси X
    int TargetX = 0;

    //!Целевая координата по оси Y
    int TargetY = 0;

    //!R-компонента цвета
    uint8_t ColorR = 0;

    //!G-компонента цвета
    uint8_t ColorG = 0;

    //!B-компонента цвета
    uint8_t ColorB = 0;

    //!Значение альфа-канала
    uint8_t ColorA = 0;

public:
    CGUILine(int x, int y, int targetX, int targetY, int polygoneColor);
    virtual ~CGUILine();

    virtual void Draw(bool checktrans = false);
};

#endif

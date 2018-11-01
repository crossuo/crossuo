/***********************************************************************************
**
** GUIGumppicTiled.h
**
** Компонента для отображения повторяющихся гампов
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef GUIGUMPPICTILED_H
#define GUIGUMPPICTILED_H

class CGUIGumppicTiled : public CGUIDrawObject
{
    //!Ширина
    int Width = 0;

    //!Высота
    int Height = 0;

public:
    CGUIGumppicTiled(uint16_t graphic, int x, int y, int width, int height);
    virtual ~CGUIGumppicTiled();

    virtual Wisp::CSize GetSize() { return Wisp::CSize(Width, Height); }

    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};

#endif

/***********************************************************************************
**
** GUITilepicScaled.h
**
** Компонента для отображения картинки статики (масштабированная
**
** Copyright (C) December 2016 Hotride
**
************************************************************************************
*/

#ifndef GUITILEPICSCALED_H
#define GUITILEPICSCALED_H

class CGUITilepicScaled : public CGUITilepic
{
    int Width = 0;
    int Height = 0;

public:
    CGUITilepicScaled(uint16_t graphic, uint16_t color, int x, int y, int width, int height);
    virtual ~CGUITilepicScaled();

    virtual void Draw(bool checktrans = false);
    virtual bool Select() { return false; }
};

#endif

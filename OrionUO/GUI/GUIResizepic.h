/***********************************************************************************
**
** GUIResizepic.h
**
** Компонента для отображения фона
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef GUIRESIZEPIC_H
#define GUIRESIZEPIC_H

class CGUIResizepic : public CGUIPolygonal
{
public:
    CGUIResizepic(int serial, ushort graphic, int x, int y, int width, int height);
    virtual ~CGUIResizepic();

    virtual Wisp::CSize GetSize() { return Wisp::CSize(Width, Height); }

    virtual void PrepareTextures();

    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};

#endif

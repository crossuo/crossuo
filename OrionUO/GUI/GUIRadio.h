/***********************************************************************************
**
** GUIRadio.h
**
** Компонента для отображения радио-кнопок
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef GUIRADIO_H
#define GUIRADIO_H

class CGUIRadio : public CGUICheckbox
{
public:
    CGUIRadio(
        int serial,
        uint16_t graphic,
        uint16_t graphicChecked,
        uint16_t graphicDisabled,
        int x,
        int y);
    virtual ~CGUIRadio();
};

#endif

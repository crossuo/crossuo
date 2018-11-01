/***********************************************************************************
**
** GUIHTMLButton.h
**
** Компонента для кнопок HTMLGump'а
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef GUIHTMLBUTTON_H
#define GUIHTMLBUTTON_H

class CGUIHTMLButton : public CGUIButton
{
private:
    //!Ссылка на гамп-родитель
    class CGUIHTMLGump *m_HTMLGump{ nullptr };

public:
    CGUIHTMLButton(
        class CGUIHTMLGump *htmlGump,
        int serial,
        uint16_t graphic,
        uint16_t graphicSelected,
        uint16_t graphicPressed,
        int x,
        int y);
    virtual ~CGUIHTMLButton();

    virtual void SetShaderMode();

    //!Обработка прокрутки
    void Scroll(bool up, int delay);

    virtual bool IsControlHTML() { return true; }
};

#endif

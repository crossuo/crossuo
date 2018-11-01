/***********************************************************************************
**
** GUIComboboxText.h
**
** Компонента для отображения текста комбобокса
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef GUICOMBOBOXTEXT_H
#define GUICOMBOBOXTEXT_H

class CGUIComboboxText : public CGUIText
{
public:
    CGUIComboboxText(
        uint16_t color,
        uint8_t font,
        const string &str,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);
    CGUIComboboxText(
        uint16_t color,
        uint8_t font,
        const wstring &str,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);
    virtual ~CGUIComboboxText();

    virtual void OnMouseEnter();
    virtual void OnMouseExit();
};

#endif

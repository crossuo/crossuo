/***********************************************************************************
**
** GUIText.h
**
** Компонента для отображения текста
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef GUITEXT_H
#define GUITEXT_H

class CGUIText : public CBaseGUI
{
public:
    CGUIText(uint16_t color, int x, int y);
    virtual ~CGUIText();

    //!Текстура текста
    CGLTextTexture m_Texture{ CGLTextTexture() };

    virtual Wisp::CSize GetSize() { return Wisp::CSize(m_Texture.Width, m_Texture.Height); }

    //!Создать обычную текстуру текста
    void CreateTextureA(
        uint8_t font,
        const string &str,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);

    //!Создать юникодовскую текстуру текста
    void CreateTextureW(
        uint8_t font,
        const wstring &str,
        uint8_t cell = 30,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t flags = 0);

    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};

#endif

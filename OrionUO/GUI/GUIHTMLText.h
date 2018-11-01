/***********************************************************************************
**
** GUIHTMLText.h
**
** Компонента текста с возможностью перехода по HTML-ссылкам для HTMLGump'а
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#ifndef GUIHTMLTEXT_H
#define GUIHTMLTEXT_H

class CGUIHTMLText : public CBaseGUI
{
public:
    //!Индекс текста
    uint32_t TextID = 0;

    //!Начальный цвет текста
    uint32_t HTMLStartColor = 0;

    //!Текст
    wstring Text = L"";

    //!Шрифт
    uint8_t Font = 0;

    //!Ориентация текста
    TEXT_ALIGN_TYPE Align = TS_LEFT;

    //!Флаги текста
    uint16_t TextFlags = 0;

    //!Ширина текста
    int Width = 0;

    CGUIHTMLText(
        int index,
        uint8_t font,
        uint16_t color,
        int x,
        int y,
        int width = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t textFlags = 0,
        int htmlStartColor = 0xFFFFFFFF);
    virtual ~CGUIHTMLText();

    //!Текстура текста
    CGLHTMLTextTexture m_Texture{ CGLHTMLTextTexture() };

    virtual Wisp::CSize GetSize() { return Wisp::CSize(m_Texture.Width, m_Texture.Height); }

    //!Создать текстуру текста
    void CreateTexture(bool backgroundCanBeColored);

    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};

#endif

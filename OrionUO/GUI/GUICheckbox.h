// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUIDrawObject.h"
#include "../plugin/enumlist.h"

class CGUICheckbox : public CGUIDrawObject
{
public:
    //!ИД картинки в нажатом состоянии
    uint16_t GraphicChecked = 0;

    //!ИД картинки в выбранном состоянии
    uint16_t GraphicSelected = 0;

    //!ИД картинки в отключенном состоянии
    uint16_t GraphicDisabled = 0;

    //!Состояние компоненты
    bool Checked = false;

    //!Позиция текста
    SLIDER_TEXT_POSITION TextPosition = STP_RIGHT;

    //!Координата текста по оси X
    int TextX = 0;

    //!Координата текста по оси Y
    int TextY = 0;

    //!Стандартное смещение текста
    int DefaultTextOffset = 2;

protected:
    CGLTextTexture Text{ CGLTextTexture() };

    void UpdateTextPosition();

public:
    CGUICheckbox(
        int serial,
        uint16_t graphic,
        uint16_t graphicChecked,
        uint16_t graphicDisabled,
        int x,
        int y);
    virtual ~CGUICheckbox();

    //!Установить текст
    void SetTextParameters(
        uint8_t font,
        const wstring &text,
        uint16_t color,
        SLIDER_TEXT_POSITION textPosition = STP_RIGHT,
        int textWidth = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t textFlags = 0);
    void SetTextParameters(
        uint8_t font,
        const string &text,
        uint16_t color,
        SLIDER_TEXT_POSITION textPosition = STP_RIGHT,
        int textWidth = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t textFlags = 0);

    virtual void PrepareTextures();
    virtual uint16_t GetDrawGraphic();

    virtual void Draw(bool checktrans = false);
    virtual bool Select();

    virtual void OnMouseEnter();
    virtual void OnMouseExit();
};

// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUIDrawObject.h"

class CGUIButton : public CGUIDrawObject
{
public:
    //!ИД картинки для состояния, когда мышка находится над кнопкой
    uint16_t GraphicSelected = 0;

    //!ИД картинки для зажатого состояния
    uint16_t GraphicPressed = 0;

    //!Индекс страницы, если указан -1 то используется событие OnButton для гампа с идентификатором кнопки, если отличное значение - переход на страницу без вызова события
    int ToPage = -1;

    //!Флаг обработки зажатого состояния кнопки каждую итерацию рендера
    bool ProcessPressedState = false;

    CGUIButton(
        int serial,
        uint16_t graphic,
        uint16_t graphicSelected,
        uint16_t graphicPressed,
        int x,
        int y);
    virtual ~CGUIButton();

    virtual void PrepareTextures();
    virtual uint16_t GetDrawGraphic();

    virtual void OnMouseEnter();
    virtual void OnMouseExit();
};

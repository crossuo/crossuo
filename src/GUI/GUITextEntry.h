// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "BaseGUI.h"
#include "../TextEngine/EntryText.h"

class CGUITextEntry : public CBaseGUI
{
public:
    //!Цвет текста в выбранном состоянии
    uint16_t ColorSelected = 0;

    //!Цвет текста в состоянии фокуса
    uint16_t ColorFocused = 0;

    //!Текст в юникоде
    bool Unicode = false;

    //!Шрифт
    uint8_t Font = 0;

    //!Ориентация текста
    TEXT_ALIGN_TYPE Align = TS_LEFT;

    //!Флаги текста
    uint16_t TextFlags = 0;

    //!Производить выбор объекта по серийнику может быть выбран фоном, хит-боксом и т.п.)
    bool CheckOnSerial = false;

    //!Только для чтения
    bool ReadOnly = false;

    //!Флаг фокуса
    bool Focused = false;

    //!Флаг использования глобального цвета
    bool UseGlobalColor = false;

    //!R-компонента глобального OGL цвета
    uint8_t GlobalColorR = 0;

    //!G-компонента глобального OGL цвета
    uint8_t GlobalColorG = 0;

    //!B-компонента глобального OGL цвета
    uint8_t GlobalColorB = 0;

    //!Альфа-канал глобального OGL цвета
    uint8_t GlobalColorA = 0;

    //!R-компонента глобального OGL цвета когда компонента выбрана
    uint8_t GlobalColorSelectedR = 0;

    //!G-компонента глобального OGL цвета когда компонента выбрана
    uint8_t GlobalColorSelectedG = 0;

    //!B-компонента глобального OGL цвета когда компонента выбрана
    uint8_t GlobalColorSelectedB = 0;

    //!Альфа-канал глобального OGL цвета когда компонента выбрана
    uint8_t GlobalColorSelectedA = 0;

    //!R-компонента глобального OGL цвета когда компонента находится в фокусе
    uint8_t GlobalColorFocusedR = 0;

    //!G-компонента глобального OGL цвета когда компонента находится в фокусе
    uint8_t GlobalColorFocusedG = 0;

    //!B-компонента глобального OGL цвета когда компонента находится в фокусе
    uint8_t GlobalColorFocusedB = 0;

    //!Альфа-канал глобального OGL цвета когда компонента находится в фокусе
    uint8_t GlobalColorFocusedA = 0;

    //!Смещение текста когда компонента находится в фокусе
    char FocusedOffsetY = 0;

    CGUITextEntry(
        int serial,
        uint16_t color,
        uint16_t colorSelected,
        uint16_t colorFocused,
        int x,
        int y,
        int maxWidth = 0,
        bool unicode = true,
        uint8_t font = 0,
        TEXT_ALIGN_TYPE align = TS_LEFT,
        uint16_t textFlags = 0,
        int maxLength = 0);
    virtual ~CGUITextEntry();

    //!Объект класса для ввода текста
    class CEntryText m_Entry;

    virtual void PrepareTextures();

    virtual bool EntryPointerHere();

    virtual CSize GetSize();

    //!Установить глобальный OGL цвет перед отрисовкой текста
    void SetGlobalColor(bool use, int color, int selected, int focused);

    //!Нажатие на компоненту
    void OnClick(CGump *gump, int x, int y);

    virtual void OnMouseEnter();
    virtual void OnMouseExit();

    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};

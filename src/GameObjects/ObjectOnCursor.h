// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

struct STATIC_TILES;

class CObjectOnCursor
{
public:
    //!Активен
    bool Enabled = false;
    //!Сброшен
    bool Dropped = false;
    //!Серийник объекта
    uint32_t Serial = 0;
    //!Индекс картинки
    uint16_t Graphic = 0;
    //!Цвет
    uint16_t Color = 0;
    //!Количество для перемещения
    uint16_t Count = 0;
    //!Количество ранее в стеке
    uint16_t TotalCount = 0;
    //!Слой
    uint8_t Layer = 0;
    //!Флаги
    uint8_t Flags = 0;
    //!Контейнер
    uint32_t Container = 0;
    //!Координата X
    uint16_t X = 0;
    //!Координата Y
    uint16_t Y = 0;
    //!Координата Z
    char Z = 0;
    //!Это игровая фигура
    bool IsGameFigure = false;
    //!Указатель на тайлдату для объекта
    STATIC_TILES *TiledataPtr = nullptr; //!Обновлен ли предмет в мире
    bool UpdatedInWorld = false;

    CObjectOnCursor() {}
    virtual ~CObjectOnCursor() {}

    void Clear();

    uint16_t GetDrawGraphic(bool &doubleDraw);
};

extern CObjectOnCursor g_ObjectInHand;

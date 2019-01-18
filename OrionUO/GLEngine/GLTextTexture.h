// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GLTexture.h"

struct WEB_LINK_RECT
{
    //Индекс ссылки
    uint16_t LinkID;

    //Начало ссылки относительно начальных координат текста
    int StartX;
    int StartY;

    //Конец ссылки относительно начальных координат текста
    int EndX;
    int EndY;
};

class CGLTextTexture : public CGLTexture
{
public:
    int LinesCount = 0;

    CGLTextTexture();
    virtual ~CGLTextTexture();

    bool Empty() { return (Texture == 0); }

    virtual void Clear();

    //Отрисовать текстуру
    virtual void Draw(int x, int y, bool checktrans = false);

    //Очистка веб-ссылки
    virtual void ClearWebLink() {}

    //Добавиление веб-ссылки
    virtual void AddWebLink(WEB_LINK_RECT &wl) {}

    //Проверка веб-ссылки под мышкой
    virtual uint16_t WebLinkUnderMouse(int x, int y) { return 0; }
};

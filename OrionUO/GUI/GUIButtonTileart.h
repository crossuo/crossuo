// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUIButton.h"

class CGUIButtonTileart : public CGUIButton
{
    //!ИД картинки статики
    uint16_t TileGraphic = 0;

    //!Цвет картинки статики
    uint16_t TileColor = 0;

    //!Координата по оси X для отображения статики
    int TileX = 0;

    //!Координата по оси Y для отображения статики
    int TileY = 0;

public:
    CGUIButtonTileart(
        int serial,
        uint16_t graphic,
        uint16_t graphicSelected,
        uint16_t graphicPressed,
        int x,
        int y,
        uint16_t tileGraphic,
        uint16_t tileColor,
        int tileX,
        int tileY);
    virtual ~CGUIButtonTileart();

    virtual CSize GetSize();

    virtual void PrepareTextures();

    virtual void Draw(bool checktrans = false);
    virtual bool Select();
};

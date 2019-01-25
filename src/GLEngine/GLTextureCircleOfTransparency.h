// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GLTexture.h"

class CGLTextureCircleOfTransparency : public CGLTexture
{
public:
    int Radius = 0;
    int X = 0;
    int Y = 0;

    CGLTextureCircleOfTransparency();
    virtual ~CGLTextureCircleOfTransparency();

    //Создать текстуру
    static void CreatePixels(int radius, short &width, short &height, vector<uint32_t> &pixels);

    //Создать текстуру
    bool Create(int radius);

    //Отрисовать текстуру
    virtual void Draw(int x, int y, bool checktrans = false);

    //Отрисовать текстуру
    void Redraw();
};

extern CGLTextureCircleOfTransparency g_CircleOfTransparency;

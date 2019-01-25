// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GUIText.h"

class CGUIGenericText : public CGUIText
{
public:
    //!Индекс текста
    uint32_t TextID = 0;

    //!Максимальная ширина (для CroppedText)
    int MaxWidth = 0;

    CGUIGenericText(int index, uint16_t color, int x, int y, int maxWidth = 0);
    virtual ~CGUIGenericText();

    void CreateTexture(const wstring &str);
};

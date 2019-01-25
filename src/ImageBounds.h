// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

class CRenderTextObject;
class CTextData;

class CImageBounds
{
public:
    int X = 0;
    int Y = 0;
    int Width = 0;
    int Height = 0;

    CImageBounds(int x, int y, int width, int height);
    virtual ~CImageBounds();

    bool InRect(const CImageBounds &ib);
};

class CTextImageBounds : public CImageBounds
{
public:
    CTextImageBounds(int x, int y, int width, int height, CRenderTextObject *text);
    CTextImageBounds(class CTextData *text);
    virtual ~CTextImageBounds();

    CRenderTextObject *Text{ nullptr };
};

extern CImageBounds g_PlayerRect;

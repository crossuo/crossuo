// MIT License
// Copyright (C) August 2016 Hotride

#include "ImageBounds.h"
#include "TextEngine/TextData.h"

CImageBounds g_PlayerRect(0, 0, 0, 0);

CImageBounds::CImageBounds(int x, int y, int width, int height)
    : X(x)
    , Y(y)
    , Width(width)
    , Height(height)
{
}

CImageBounds::~CImageBounds()
{
}

bool CImageBounds::InRect(const CImageBounds &ib)
{
    DEBUG_TRACE_FUNCTION;
    bool inRect = false;

    if (X < ib.X)
    {
        if (ib.X < X + Width)
        {
            inRect = true;
        }
    }
    else
    {
        if (X < ib.X + ib.Width)
        {
            inRect = true;
        }
    }

    if (inRect)
    {
        if (Y < ib.Y)
        {
            inRect = ib.Y < Y + Height;
        }
        else
        {
            inRect = Y < ib.Y + ib.Height;
        }
    }

    return inRect;
}

CTextImageBounds::CTextImageBounds(int x, int y, int width, int height, CRenderTextObject *text)
    : CImageBounds(x, y, width, height)
    , Text(text)
{
}

CTextImageBounds::CTextImageBounds(CTextData *text)
    : CImageBounds(text->RealDrawX, text->RealDrawY, text->m_Texture.Width, text->m_Texture.Height)
    , Text(text)
{
}

CTextImageBounds::~CTextImageBounds()
{
    Text = nullptr;
}

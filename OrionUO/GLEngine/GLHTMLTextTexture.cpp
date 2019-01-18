// MIT License
// Copyright (C) August 2016 Hotride

#include "../Managers/MouseManager.h"

CGLHTMLTextTexture::CGLHTMLTextTexture()
{
}

CGLHTMLTextTexture::~CGLHTMLTextTexture()
{
    DEBUG_TRACE_FUNCTION;
    m_WebLinkRect.clear();
}

uint16_t CGLHTMLTextTexture::WebLinkUnderMouse(int x, int y)
{
    DEBUG_TRACE_FUNCTION;
    x = g_MouseManager.Position.X - x;
    y = g_MouseManager.Position.Y - y;

    for (auto it = m_WebLinkRect.begin(); it != m_WebLinkRect.end(); ++it)
    {
        if (y >= (*it).StartY && y < (*it).StartY + (*it).EndY)
        {
            if (x >= (*it).StartX && x < (*it).StartX + (*it).EndX)
            {
                return it->LinkID;
            }
        }
    }

    return 0;
}

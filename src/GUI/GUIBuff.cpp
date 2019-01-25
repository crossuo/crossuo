// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIBuff.h"
#include "../CrossUO.h"

CGUIBuff::CGUIBuff(uint16_t graphic, int timer, const wstring &text)
    : CGUIDrawObject(GOT_BUFF, 0, graphic, 0, 0, 0)
    , Timer(timer)
    , Text(text)
{
}

CGUIBuff::~CGUIBuff()
{
}

void CGUIBuff::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;

    CGLTexture *th = g_Game.ExecuteGump(GetDrawGraphic());
    if (th != nullptr)
    {
        glColor4ub(0xFF, 0xFF, 0xFF, Alpha);
        th->Draw(m_X, m_Y, checktrans);
    }
}

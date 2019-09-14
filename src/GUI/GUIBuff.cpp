// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIBuff.h"
#include "../CrossUO.h"

CGUIBuff::CGUIBuff(uint16_t graphic, int timer, const std::wstring &text)
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
    auto spr = g_Game.ExecuteGump(GetDrawGraphic());
    if (spr != nullptr && spr->Texture)
    {
        glColor4ub(0xFF, 0xFF, 0xFF, Alpha);
        spr->Texture->Draw(m_X, m_Y, checktrans);
    }
}

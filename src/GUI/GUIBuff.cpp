// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIBuff.h"
#include "../CrossUO.h"
#include "../Renderer/RenderAPI.h"
#include "../Utility/PerfMarker.h"

extern RenderCmdList *g_renderCmdList;

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
    ScopedPerfMarker(__FUNCTION__);
    DEBUG_TRACE_FUNCTION;
    auto spr = g_Game.ExecuteGump(GetDrawGraphic());
    if (spr != nullptr && spr->Texture)
    {
#ifndef NEW_RENDERER_ENABLED
        glColor4ub(0xFF, 0xFF, 0xFF, Alpha);
#else
        RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ { 1.f, 1.f, 1.f, Alpha / 255.f } });
#endif
        spr->Texture->Draw(m_X, m_Y, checktrans);
    }
}

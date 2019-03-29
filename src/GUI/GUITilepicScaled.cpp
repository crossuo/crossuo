// MIT License
// Copyright (C) December 2016 Hotride

#include "GUITilepicScaled.h"
#include "../CrossUO.h"

CGUITilepicScaled::CGUITilepicScaled(
    uint16_t graphic, uint16_t color, int x, int y, int width, int height)
    : CGUITilepic(graphic, color, x, y)
{
}

CGUITilepicScaled::~CGUITilepicScaled()
{
}

void CGUITilepicScaled::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    auto spr = g_Game.ExecuteStaticArt(Graphic);
    if (spr != nullptr && spr->Texture != nullptr)
    {
        SetShaderMode();
        g_GL_Draw(*spr->Texture, m_X, m_Y);
    }
}

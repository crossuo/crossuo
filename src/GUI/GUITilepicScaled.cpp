// MIT License
// Copyright (C) December 2016 Hotride

#include "GUITilepicScaled.h"
#include "../CrossUO.h"

CGUITilepicScaled::CGUITilepicScaled(
    uint16_t graphic, uint16_t color, int x, int y, int width, int height)
    : CGUITilepic(graphic, color, x, y)
    , Width(width)
    , Height(height)
{
}

CGUITilepicScaled::~CGUITilepicScaled()
{
}

void CGUITilepicScaled::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    CGLTexture *th = g_Game.ExecuteStaticArt(Graphic);
    if (th != nullptr)
    {
        SetShaderMode();
        g_GL_Draw(*th, m_X, m_Y);
    }
}

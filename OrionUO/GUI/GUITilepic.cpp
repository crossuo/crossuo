// MIT License
// Copyright (C) August 2016 Hotride

#include "GUITilepic.h"
#include "../OrionUO.h"
#include "../Point.h"

CGUITilepic::CGUITilepic(uint16_t graphic, uint16_t color, int x, int y)
    : CGUIDrawObject(GOT_TILEPIC, 0, graphic, color, x, y)
{
}

CGUITilepic::~CGUITilepic()
{
}

CSize CGUITilepic::GetSize()
{
    DEBUG_TRACE_FUNCTION;
    CSize size;

    CGLTexture *th = g_Orion.ExecuteStaticArt(Graphic);

    if (th != nullptr)
    {
        size.Width = th->Width;
        size.Height = th->Height;
    }

    return size;
}

void CGUITilepic::PrepareTextures()
{
    DEBUG_TRACE_FUNCTION;
    g_Orion.ExecuteStaticArt(Graphic);
}

void CGUITilepic::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    CGLTexture *th = g_Orion.ExecuteStaticArt(Graphic);

    if (th != nullptr)
    {
        SetShaderMode();

        th->Draw(m_X, m_Y, checktrans);
    }
}

bool CGUITilepic::Select()
{
    DEBUG_TRACE_FUNCTION;
    //if (CGUIDrawObject::Select())
    //	return true;
    CGLTexture *th = g_Orion.m_StaticDataIndex[Graphic].Texture;

    if (th != nullptr)
    {
        return th->Select(m_X, m_Y, !CheckPolygone);
    }

    return false;
}

// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/***********************************************************************************
**
** GUITilepic.cpp
**
** Copyright (C) August 2016 Hotride
**
************************************************************************************
*/

#include "stdafx.h"

CGUITilepic::CGUITilepic(uint16_t graphic, uint16_t color, int x, int y)
    : CGUIDrawObject(GOT_TILEPIC, 0, graphic, color, x, y)
{
}

CGUITilepic::~CGUITilepic()
{
}

Wisp::CSize CGUITilepic::GetSize()
{
    DEBUG_TRACE_FUNCTION;
    Wisp::CSize size;

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
        return th->Select(m_X, m_Y, !CheckPolygone);

    return false;
}


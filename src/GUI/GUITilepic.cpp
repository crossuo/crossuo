// MIT License
// Copyright (C) August 2016 Hotride
// AGPLv3 License
// Copyright (c) 2020 Danny Angelo Carminati Grein

#include "GUITilepic.h"
#include <xuocore/uodata.h>
#include "../CrossUO.h"
#include "../Sprite.h"
#include "../Point.h"
#include "../Utility/PerfMarker.h"

CGUITilepic::CGUITilepic(uint16_t graphic, uint16_t color, int x, int y, bool doubleDraw)
    : CGUIDrawObject(GOT_TILEPIC, 0, graphic, color, x, y)
    , DoubleDraw(doubleDraw)
{
}

CSize CGUITilepic::GetSize()
{
    CSize size;
    auto spr = g_Game.ExecuteStaticArt(Graphic);
    if (spr != nullptr)
    {
        size.Width = spr->Width;
        size.Height = spr->Height;
    }
    return size;
}

void CGUITilepic::PrepareTextures()
{
    g_Game.ExecuteStaticArt(Graphic);
}

void CGUITilepic::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);
    auto spr = g_Game.ExecuteStaticArt(Graphic);
    if (spr != nullptr && spr->Texture)
    {
        SetShaderMode();
        spr->Texture->Draw(m_X, m_Y, checktrans);
        if (DoubleDraw)
        {
            spr->Texture->Draw(m_X + 5, m_Y + 5, checktrans);
        }
    }
}

bool CGUITilepic::Select()
{
    auto spr = (CSprite *)g_Index.m_Static[Graphic].UserData;
    if (spr != nullptr)
    {
        return spr->Select(m_X, m_Y, !BoundingBoxCheck);
    }
    return false;
}

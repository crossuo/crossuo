// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GUIButtonTileart.h"
#include "../CrossUO.h"
#include "../Sprite.h"
#include "../Point.h"
#include "../Utility/PerfMarker.h"

CGUIButtonTileart::CGUIButtonTileart(
    int serial,
    uint16_t graphic,
    uint16_t graphicSelected,
    uint16_t graphicPressed,
    int x,
    int y,
    uint16_t tileGraphic,
    uint16_t tileColor,
    int tileX,
    int tileY)
    : CGUIButton(serial, graphic, graphicSelected, graphicPressed, x, y)
    , TileGraphic(tileGraphic)
    , TileX(tileX)
    , TileY(tileY)
{
    Type = GOT_BUTTONTILEART;
}

CGUIButtonTileart::~CGUIButtonTileart()
{
}

CSize CGUIButtonTileart::GetSize()
{
    CSize gumpSize = CGUIDrawObject::GetSize();
    CSize tileSize;

    auto spr = g_Game.ExecuteStaticArt(TileGraphic);
    if (spr != nullptr)
    {
        tileSize.Width = spr->Width;
        tileSize.Height = spr->Height;
    }

    int startX = m_X;
    int endX = m_X + gumpSize.Width;
    if (TileX < startX)
    {
        startX = TileX;
    }

    if (TileX + tileSize.Width > endX)
    {
        endX = TileX + tileSize.Width;
    }

    int startY = m_Y;
    int endY = m_Y + gumpSize.Height;
    if (TileY < startY)
    {
        startY = TileY;
    }

    if (TileY + tileSize.Height > endY)
    {
        endY = TileY + tileSize.Height;
    }

    return CSize(abs(endX) - abs(startX), abs(endY) - abs(startY));
}

void CGUIButtonTileart::PrepareTextures()
{
    CGUIButton::PrepareTextures();
    g_Game.ExecuteStaticArt(TileGraphic);
}

void CGUIButtonTileart::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);

    CGUIDrawObject::Draw(checktrans);
    auto spr = g_Game.ExecuteStaticArt(TileGraphic);
    if (spr != nullptr && spr->Texture)
    {
        SetShaderMode();
        spr->Texture->Draw(m_X, m_Y, checktrans);
    }
}

bool CGUIButtonTileart::Select()
{
    if (CGUIDrawObject::Select())
    {
        return true;
    }
    auto spr = g_Game.ExecuteStaticArt(TileGraphic);
    if (spr != nullptr)
    {
        return spr->Select(m_X, m_Y, !BoundingBoxCheck);
    }
    return false;
}

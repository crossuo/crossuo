// MIT License
// Copyright (C) August 2016 Hotride

#include "StaticObject.h"
#include "GameCharacter.h"
#include "../CrossUO.h"
#include "../SelectedObject.h"
#include "../Utility/PerfMarker.h"

CStaticObject::CStaticObject(int serial, uint16_t graphic, uint16_t color, short x, short y, char z)
    : CRenderStaticObject(ROT_STATIC_OBJECT, serial, graphic, color, x, y, z)
{
    OriginalGraphic = graphic;
    UpdateGraphicBySeason();

    //if (!color)
    //	m_Color = m_TiledataPtr->Hue;

    m_TextControl->MaxSize = 1;

    g_StaticsObjectsCount++;
}

void CStaticObject::UpdateGraphicBySeason()
{
    //uint16_t graphic = Graphic;

    Graphic = g_Game.GetSeasonGraphic(OriginalGraphic);

    //if (Graphic != graphic)
    {
        Vegetation = g_Game.IsVegetation(Graphic);
    }

    NoDrawTile = IsNoDrawTile(Graphic);
}

void CStaticObject::Draw(int x, int y)
{
    ScopedPerfMarker(__FUNCTION__);

    RenderGraphic = Graphic;

    if (g_DeveloperMode == DM_DEBUGGING && g_SelectedObject.Object == this)
    {
        RenderColor = SELECT_STATIC_COLOR;
    }
    else
    {
        RenderColor = Color;
    }

    CRenderStaticObject::Draw(x, y);
}

void CStaticObject::Select(int x, int y)
{
    RenderGraphic = Graphic;

    CRenderStaticObject::Select(x, y);
}

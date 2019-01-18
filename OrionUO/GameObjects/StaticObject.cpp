// MIT License
// Copyright (C) August 2016 Hotride

#include "StaticObject.h"
#include "GameCharacter.h"
#include "../OrionUO.h"
#include "../SelectedObject.h"

CStaticObject::CStaticObject(int serial, uint16_t graphic, uint16_t color, short x, short y, char z)
    : CRenderStaticObject(ROT_STATIC_OBJECT, serial, graphic, color, x, y, z)
{
    DEBUG_TRACE_FUNCTION;
    OriginalGraphic = graphic;
    UpdateGraphicBySeason();

    //if (!color)
    //	m_Color = m_TiledataPtr->Hue;

    m_TextControl->MaxSize = 1;

#if UO_DEBUG_INFO != 0
    g_StaticsObjectsCount++;
#endif //UO_DEBUG_INFO!=0
}

void CStaticObject::UpdateGraphicBySeason()
{
    DEBUG_TRACE_FUNCTION;
    //uint16_t graphic = Graphic;

    Graphic = g_Orion.GetSeasonGraphic(OriginalGraphic);

    //if (Graphic != graphic)
    {
        Vegetation = g_Orion.IsVegetation(Graphic);
    }

    NoDrawTile = IsNoDrawTile(Graphic);
}

void CStaticObject::Draw(int x, int y)
{
    DEBUG_TRACE_FUNCTION;
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
    DEBUG_TRACE_FUNCTION;
    RenderGraphic = Graphic;

    CRenderStaticObject::Select(x, y);
}

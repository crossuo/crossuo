// MIT License
// Copyright (C) August 2016 Hotride

#include "MultiObject.h"
#include "CustomHouseMultiObject.h"
#include "../CrossUO.h"
#include "../SelectedObject.h"
#include "../Managers/CustomHousesManager.h"
#include "../Gumps/GumpCustomHouse.h"

CMultiObject::CMultiObject(uint16_t graphic, short x, short y, char z, int flags)
    : CRenderStaticObject(ROT_MULTI_OBJECT, 0, graphic, 0, x, y, z)
    , OnTarget(flags == 2)
{
    DEBUG_TRACE_FUNCTION;
    OriginalGraphic = graphic;
    UpdateGraphicBySeason();

#if UO_DEBUG_INFO != 0
    g_MultiObjectsCount++;
#endif //UO_DEBUG_INFO!=0
}

CMultiObject::~CMultiObject()
{
#if UO_DEBUG_INFO != 0
    g_MultiObjectsCount--;
#endif //UO_DEBUG_INFO!=0
}

void CMultiObject::UpdateGraphicBySeason()
{
    DEBUG_TRACE_FUNCTION;
    //uint16_t graphic = Graphic;

    Graphic = g_Game.GetSeasonGraphic(OriginalGraphic);

    //if (Graphic != graphic)
    {
        Vegetation = g_Game.IsVegetation(Graphic);
    }
}

void CMultiObject::Draw(int x, int y)
{
    DEBUG_TRACE_FUNCTION;

    uint16_t color = Color;

    if (State != 0)
    {
        if ((State & CHMOF_IGNORE_IN_RENDER) != 0)
        {
            return;
        }

        if ((State & CHMOF_INCORRECT_PLACE) != 0)
        {
            color = 0x002B;
        }

        if ((State & CHMOF_TRANSPARENT) != 0)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(1.0f, 1.0f, 1.0f, 0.75f);

            g_Game.DrawStaticArt(Graphic, color, x, y);

            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            glDisable(GL_BLEND);

#if UO_DEBUG_INFO != 0
            g_RenderedObjectsCountInGameWindow++;
#endif

            return;
        }
    }

#if UO_DEBUG_INFO != 0
    g_RenderedObjectsCountInGameWindow++;
#endif

    if (OnTarget) //Мульти на таргете
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR);

        g_Game.DrawStaticArt(Graphic, color, x, y);

        glDisable(GL_BLEND);
    }
    else
    {
        RenderGraphic = Graphic;

        if (g_DeveloperMode == DM_DEBUGGING && g_SelectedObject.Object == this)
        {
            RenderColor = SELECT_MULTI_COLOR;
        }
        else
        {
            RenderColor = color;
        }

        CRenderStaticObject::Draw(x, y);
    }
}

void CMultiObject::Select(int x, int y)
{
    DEBUG_TRACE_FUNCTION;
    if (!OnTarget)
    {
        if (State != 0)
        {
            if ((State & CHMOF_IGNORE_IN_RENDER) != 0)
            {
                return;
            }
            if (((State & CHMOF_TRANSPARENT) != 0) && ((State & CHMOF_GENERIC_INTERNAL) == 0))
            {
                if (g_CustomHouseGump == nullptr || (g_CustomHouseGump->SelectedGraphic == 0u))
                {
                    return;
                }
            }
        }

        RenderGraphic = Graphic;

        CRenderStaticObject::Select(x, y);
    }
}

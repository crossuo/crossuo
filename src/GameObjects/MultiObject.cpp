// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "MultiObject.h"
#include "CustomHouseMultiObject.h"
#include "../CrossUO.h"
#include "../SelectedObject.h"
#include "../Managers/CustomHousesManager.h"
#include "../Gumps/GumpCustomHouse.h"
#include "../Renderer/RenderAPI.h"
#include "../Utility/PerfMarker.h"

CMultiObject::CMultiObject(uint16_t graphic, short x, short y, char z, int flags)
    : CRenderStaticObject(ROT_MULTI_OBJECT, 0, graphic, 0, x, y, z)
    , OnTarget(flags == 2)
{
    OriginalGraphic = graphic;
    UpdateGraphicBySeason();

    g_MultiObjectsCount++;
}

CMultiObject::~CMultiObject()
{
    g_MultiObjectsCount--;
}

void CMultiObject::UpdateGraphicBySeason()
{
    //uint16_t graphic = Graphic;

    Graphic = g_Game.GetSeasonGraphic(OriginalGraphic);

    //if (Graphic != graphic)
    {
        Vegetation = g_Game.IsVegetation(Graphic);
    }
}

void CMultiObject::Draw(int x, int y)
{
    ScopedPerfMarker(__FUNCTION__);
    SCOPED_GL_DEBUG_MARKER_LABEL("CMultiObject::Draw");
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
            RenderAdd_SetBlend(
                g_renderCmdList,
                BlendStateCmd{ BlendFactor::BlendFactor_SrcAlpha,
                               BlendFactor::BlendFactor_OneMinusSrcAlpha });
            RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ { 1.f, 1.f, 1.f, 0.75f } });

            g_Game.DrawStaticArt(Graphic, color, x, y);

            RenderAdd_SetColor(g_renderCmdList, SetColorCmd{ g_ColorWhite });
            RenderAdd_DisableBlend(g_renderCmdList);

            g_RenderedObjectsCountInGameWindow++;

            return;
        }
    }

    g_RenderedObjectsCountInGameWindow++;

    if (OnTarget)
    {
        RenderAdd_SetBlend(
            g_renderCmdList,
            BlendStateCmd{ BlendFactor::BlendFactor_SrcColor,
                           BlendFactor::BlendFactor_OneMinusSrcColor });

        g_Game.DrawStaticArt(Graphic, color, x, y);

        RenderAdd_DisableBlend(g_renderCmdList);
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

// MIT License
// Copyright (C) August 2016 Hotride

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

    Graphic = g_Orion.GetSeasonGraphic(OriginalGraphic);

    //if (Graphic != graphic)
    {
        Vegetation = g_Orion.IsVegetation(Graphic);
    }
}

void CMultiObject::Draw(int x, int y)
{
    DEBUG_TRACE_FUNCTION;

    uint16_t color = Color;

    if (State)
    {
        if (State & CHMOF_IGNORE_IN_RENDER)
            return;

        if (State & CHMOF_INCORRECT_PLACE)
            color = 0x002B;

        if (State & CHMOF_TRANSPARENT)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(1.0f, 1.0f, 1.0f, 0.75f);

            g_Orion.DrawStaticArt(Graphic, color, x, y);

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

        g_Orion.DrawStaticArt(Graphic, color, x, y);

        glDisable(GL_BLEND);
    }
    else
    {
        RenderGraphic = Graphic;

        if (g_DeveloperMode == DM_DEBUGGING && g_SelectedObject.Object == this)
            RenderColor = SELECT_MULTI_COLOR;
        else
            RenderColor = color;

        CRenderStaticObject::Draw(x, y);
    }
}

void CMultiObject::Select(int x, int y)
{
    DEBUG_TRACE_FUNCTION;
    if (!OnTarget)
    {
        if (State)
        {
            if (State & CHMOF_IGNORE_IN_RENDER)
                return;
            else if ((State & CHMOF_TRANSPARENT) && !(State & CHMOF_GENERIC_INTERNAL))
            {
                if (g_CustomHouseGump == nullptr || !g_CustomHouseGump->SelectedGraphic)
                    return;
            }
        }

        RenderGraphic = Graphic;

        CRenderStaticObject::Select(x, y);
    }
}


// MIT License
// Copyright (C) August 2016 Hotride

#include "GumpDye.h"
#include "../Network/Packets.h"

CGumpDye::CGumpDye(uint32_t serial, int16_t x, int16_t y, uint16_t graphic)
    : CGumpSelectColor(serial, x, y, SCGS_OPT_TOOLTIP_TEXT)
{
    NoClose = true;
    GumpType = GT_DYE;
    Graphic = graphic;
}

CGumpDye::~CGumpDye()
{
}

uint16_t CGumpDye::GetCurrentColor()
{
    DEBUG_TRACE_FUNCTION;
    uint16_t startColor = m_ColorRef + 2;
    uint16_t color = 0;

    for (int y = 0; y < 10 && (color == 0u); y++)
    {
        for (int x = 0; x < 20; x++)
        {
            if (m_SelectedIndex == ID_GSC_COLORS + (x * 30 + y))
            {
                color = startColor;
                break;
            }

            startColor += 5;
        }
    }

    return color;
}

void CGumpDye::UpdateContent()
{
    DEBUG_TRACE_FUNCTION;
    CGumpSelectColor::UpdateContent();

    if (m_Tube == nullptr)
    {
        Add(new CGUIShader(&g_ColorizerShader, true));

        m_Tube = (CGUITilepic *)Add(new CGUITilepic(0x0FAB, GetCurrentColor(), 200, 58));
        m_Tube->PartialHue = true;

        Add(new CGUIShader(&g_ColorizerShader, false));
    }
    else
    {
        m_Tube->Color = GetCurrentColor();
    }
}

void CGumpDye::OnSelectColor(uint16_t color)
{
    DEBUG_TRACE_FUNCTION;
    CPacketDyeDataResponse(Serial, Graphic, color + 1).Send();
    RemoveMark = true;
}

void CGumpDye::GUMP_BUTTON_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    CGumpSelectColor::OnButton(serial);

    if (serial >= ID_GSC_COLORS && m_Tube != nullptr)
    {
        m_Tube->Color = GetCurrentColor();
    }
}

void CGumpDye::GUMP_SLIDER_CLICK_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    OnSliderMove(serial);
}

void CGumpDye::GUMP_SLIDER_MOVE_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    CGumpSelectColor::OnSliderMove(serial);

    if (m_Tube != nullptr)
    {
        m_Tube->Color = GetCurrentColor();
    }
}

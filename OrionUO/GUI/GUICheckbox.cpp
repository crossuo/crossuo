// MIT License
// Copyright (C) August 2016 Hotride

#include "GUICheckbox.h"
#include "../OrionUO.h"
#include "../SelectedObject.h"
#include "../Managers/FontsManager.h"
#include "../Managers/MouseManager.h"
#include "../Gumps/Gump.h"

CGUICheckbox::CGUICheckbox(
    int serial, uint16_t graphic, uint16_t graphicChecked, uint16_t graphicDisabled, int x, int y)
    : CGUIDrawObject(GOT_CHECKBOX, serial, graphic, 0, x, y)
    , GraphicChecked(graphicChecked)
    , GraphicSelected(graphic)
    , GraphicDisabled(graphicDisabled)
{
}

CGUICheckbox::~CGUICheckbox()
{
    DEBUG_TRACE_FUNCTION;
    Text.Clear();
}

void CGUICheckbox::SetTextParameters(
    uint8_t font,
    const wstring &text,
    uint16_t color,
    SLIDER_TEXT_POSITION textPosition,
    int textWidth,
    TEXT_ALIGN_TYPE align,
    uint16_t textFlags)
{
    DEBUG_TRACE_FUNCTION;
    TextPosition = textPosition;
    g_FontManager.GenerateW(font, Text, text, color, 30, textWidth, align, textFlags);
    UpdateTextPosition();
}

void CGUICheckbox::SetTextParameters(
    uint8_t font,
    const string &text,
    uint16_t color,
    SLIDER_TEXT_POSITION textPosition,
    int textWidth,
    TEXT_ALIGN_TYPE align,
    uint16_t textFlags)
{
    DEBUG_TRACE_FUNCTION;
    TextPosition = textPosition;
    g_FontManager.GenerateA(font, Text, text, color, textWidth, align, textFlags);
    UpdateTextPosition();
}

void CGUICheckbox::UpdateTextPosition()
{
    DEBUG_TRACE_FUNCTION;
    int textX = m_X;
    int textY = m_Y;

    CGLTexture *th = g_Orion.ExecuteGump(Graphic);

    if (th != nullptr)
    {
        switch (TextPosition)
        {
            case STP_TOP:
            case STP_TOP_CENTER:
            {
                textY -= Text.Height - DefaultTextOffset;

                break;
            }
            case STP_BOTTOM:
            case STP_BOTTOM_CENTER:
            {
                textY += th->Height + DefaultTextOffset;

                break;
            }
            case STP_LEFT:
                //textY -= 4;
            case STP_LEFT_CENTER:
            {
                textX -= Text.Width - DefaultTextOffset;

                break;
            }
            case STP_RIGHT:
                //textY -= 4;
            case STP_RIGHT_CENTER:
            {
                textX += th->Width + DefaultTextOffset;

                break;
            }
            default:
                break;
        }

        switch (TextPosition)
        {
            case STP_TOP_CENTER:
            case STP_BOTTOM_CENTER:
            {
                int textWidth = Text.Width;
                int sliderWidth = th->Width;

                int deltaX = abs(sliderWidth - textWidth) / 2;

                if (sliderWidth > textWidth)
                {
                    textX += deltaX;
                }
                else
                {
                    textX -= deltaX;
                }

                break;
            }
            case STP_LEFT_CENTER:
            case STP_RIGHT_CENTER:
            {
                int textHeight = Text.Height;
                int sliderHeight = th->Height;

                int deltaY = abs(sliderHeight - textHeight) / 2;

                if (sliderHeight > textHeight)
                {
                    textY += deltaY;
                }
                else
                {
                    textY -= deltaY;
                }

                break;
            }
            default:
                break;
        }
    }

    TextX = textX;
    TextY = textY;
}

void CGUICheckbox::PrepareTextures()
{
    DEBUG_TRACE_FUNCTION;
    g_Orion.ExecuteGump(Graphic);
    g_Orion.ExecuteGump(GraphicChecked);
    g_Orion.ExecuteGump(GraphicDisabled);
}

uint16_t CGUICheckbox::GetDrawGraphic()
{
    DEBUG_TRACE_FUNCTION;
    uint16_t graphic = Graphic;

    if (!Enabled)
    {
        graphic = GraphicDisabled;
    }
    else if (Checked)
    {
        graphic = GraphicChecked;
    }
    else if (g_SelectedObject.Object == this)
    {
        graphic = GraphicSelected;
    }

    return graphic;
}

void CGUICheckbox::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    CGUIDrawObject::Draw(checktrans);

    Text.Draw(TextX, TextY, checktrans);
}

bool CGUICheckbox::Select()
{
    DEBUG_TRACE_FUNCTION;
    bool result = CGUIDrawObject::Select();

    if (!result && !Text.Empty())
    {
        int x = g_MouseManager.Position.X - TextX;
        int y = g_MouseManager.Position.Y - TextY;

        result = (x >= 0 && y >= 0 && x < Text.Width && y < Text.Height);
    }

    return result;
}

void CGUICheckbox::OnMouseEnter()
{
    DEBUG_TRACE_FUNCTION;
    if (Graphic != GraphicSelected && g_SelectedObject.Gump != nullptr)
    {
        g_SelectedObject.Gump->WantRedraw = true;
    }
}

void CGUICheckbox::OnMouseExit()
{
    DEBUG_TRACE_FUNCTION;
    if (Graphic != GraphicSelected && g_LastSelectedObject.Gump != nullptr)
    {
        g_LastSelectedObject.Gump->WantRedraw = true;
    }
}

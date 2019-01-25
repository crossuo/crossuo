// MIT License
// Copyright (C) September 2016 Hotride

#include "GUIMinMaxButtons.h"
#include "../CrossUO.h"
#include "../Managers/FontsManager.h"
#include "../Managers/MouseManager.h"

CGUIMinMaxButtons::CGUIMinMaxButtons(
    int serial, uint16_t graphic, int x, int y, int minValue, int maxValue, int value)
    : CBaseGUI(GOT_MINMAXBUTTONS, serial, graphic, 0, x, y)
    , MinValue(minValue)
    , MaxValue(maxValue)
    , Value(value)
{
}

CGUIMinMaxButtons::~CGUIMinMaxButtons()
{
    DEBUG_TRACE_FUNCTION;
    Text.Clear();
}

void CGUIMinMaxButtons::UpdateText()
{
    DEBUG_TRACE_FUNCTION;
    if (HaveText)
    {
        if (Unicode)
        {
            g_FontManager.GenerateW(
                Font, Text, std::to_wstring(Value), TextColor, 30, TextWidth, Align, TextFlags);
        }
        else
        {
            g_FontManager.GenerateA(
                Font, Text, std::to_string(Value), TextColor, TextWidth, Align, TextFlags);
        }

        CGLTexture *th = g_Game.ExecuteGump(Graphic);

        if (th != nullptr)
        {
            int textX = m_X;
            int textY = m_Y;

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
                    textY += 18 + DefaultTextOffset;

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
                    textX += 36 + DefaultTextOffset;

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
                    int sliderWidth = 36;

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
                    int sliderHeight = 18;

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

            TextX = textX;
            TextY = textY;
        }
    }
}

void CGUIMinMaxButtons::Scroll(int delay)
{
    DEBUG_TRACE_FUNCTION;
    if (LastScrollTime < g_Ticks && (m_ScrollMode != 0))
    {
        if (m_ScrollMode == 1)
        {
            Value += ScrollStep;
        }
        else
        {
            Value -= ScrollStep;
        }

        if (Value < MinValue)
        {
            Value = MinValue;
        }
        else if (Value > MaxValue)
        {
            Value = MaxValue;
        }

        ScrollStep++;
        LastScrollTime = g_Ticks + delay;

        UpdateText();
    }
}

void CGUIMinMaxButtons::OnClick()
{
    DEBUG_TRACE_FUNCTION;
    int x = g_MouseManager.Position.X - m_X;
    int y = g_MouseManager.Position.Y - m_Y;

    if (x >= 0 && y >= 0 && y < 18)
    {
        if (x < 18)
        {
            m_ScrollMode = 1;
        }
        else
        {
            m_ScrollMode = 2;
        }
    }
    else
    {
        m_ScrollMode = 0;
    }

    LastScrollTime = g_Ticks + 100;
    ScrollStep = BaseScrollStep;
}

void CGUIMinMaxButtons::SetTextParameters(
    bool haveText,
    SLIDER_TEXT_POSITION textPosition,
    uint8_t font,
    uint16_t color,
    bool unicode,
    int textWidth,
    TEXT_ALIGN_TYPE align,
    uint16_t textFlags)
{
    DEBUG_TRACE_FUNCTION;
    HaveText = haveText;
    TextPosition = textPosition;
    Font = font;
    TextColor = color;
    Unicode = unicode;
    TextWidth = textWidth;
    Align = align;
    TextFlags = textFlags;

    UpdateText();
}

void CGUIMinMaxButtons::PrepareTextures()
{
    DEBUG_TRACE_FUNCTION;
    g_Game.ExecuteGump(Graphic);
    g_Game.ExecuteGump(Graphic + 1);
}

void CGUIMinMaxButtons::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);

    for (int i = 0; i < 2; i++)
    {
        CGLTexture *th = g_Game.ExecuteGump(Graphic + (int)i);

        if (th != nullptr)
        {
            th->Draw(m_X + ((int)i * 18), m_Y, checktrans);
        }
    }

    if (HaveText)
    {
        Text.Draw(TextX, TextY, checktrans);
    }
}

bool CGUIMinMaxButtons::Select()
{
    DEBUG_TRACE_FUNCTION;
    int x = g_MouseManager.Position.X - m_X;
    int y = g_MouseManager.Position.Y - m_Y;

    return (x >= 0 && y >= 0 && x < 36 && y < 18);
}

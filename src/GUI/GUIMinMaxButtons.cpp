// MIT License
// Copyright (C) September 2016 Hotride

#include "GUIMinMaxButtons.h"
#include "../CrossUO.h"
#include "../Managers/FontsManager.h"
#include "../Managers/MouseManager.h"
#include "../Utility/PerfMarker.h"
#include "../Renderer/RenderAPI.h"

extern RenderCmdList *g_renderCmdList;

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
    Text.Clear();
}

void CGUIMinMaxButtons::UpdateText()
{
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
                Font, Text, str_from_int(Value), TextColor, TextWidth, Align, TextFlags);
        }

        auto spr = g_Game.ExecuteGump(Graphic);
        if (spr != nullptr)
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
    g_Game.ExecuteGump(Graphic);
    g_Game.ExecuteGump(Graphic + 1);
}

void CGUIMinMaxButtons::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);

#ifndef NEW_RENDERER_ENABLED
    glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
#else
    ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
    cmd.value.asInt1 = SDM_NO_COLOR;
    RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
    for (int i = 0; i < 2; i++)
    {
        auto spr = g_Game.ExecuteGump(Graphic + (int)i);
        if (spr != nullptr && spr->Texture != nullptr)
        {
            spr->Texture->Draw(m_X + ((int)i * 18), m_Y, checktrans);
        }
    }

    if (HaveText)
    {
        Text.Draw(TextX, TextY, checktrans);
    }
}

bool CGUIMinMaxButtons::Select()
{
    const int x = g_MouseManager.Position.X - m_X;
    const int y = g_MouseManager.Position.Y - m_Y;
    return (x >= 0 && y >= 0 && x < 36 && y < 18);
}

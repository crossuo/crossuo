// MIT License
// Copyright (C) August 2016 Hotride

#include "GUISlider.h"
#include "../CrossUO.h"
#include "../Point.h"
#include "../SelectedObject.h"
#include "../Managers/FontsManager.h"
#include "../Managers/MouseManager.h"
#include "../Gumps/Gump.h"
#include "../Utility/PerfMarker.h"
#include "../Renderer/RenderAPI.h"

extern RenderCmdList *g_renderCmdList;

CGUISlider::CGUISlider(
    int serial,
    uint16_t graphic,
    uint16_t graphicSelected,
    uint16_t graphicPressed,
    uint16_t backgroundGraphic,
    bool compositeBackground,
    bool vertical,
    int x,
    int y,
    int length,
    int minValue,
    int maxValue,
    int value)
    : CBaseGUI(GOT_SLIDER, serial, graphic, 0, x, y)
    , GraphicSelected(graphicSelected)
    , GraphicPressed(graphicPressed)
    , BackgroundGraphic(backgroundGraphic)
    , CompositeBackground(compositeBackground)
    , Vertical(vertical)
    , Length(length)
    , MinValue(minValue)
    , MaxValue(maxValue)
    , Value(value)
{
    CalculateOffset();
}

CGUISlider::~CGUISlider()
{
    Text.Clear();
}

void CGUISlider::UpdateText()
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
                Font, Text, std::to_string(Value), TextColor, TextWidth, Align, TextFlags);
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
                    if (Vertical)
                    {
                        textY += Length + DefaultTextOffset;
                    }
                    else
                    {
                        textY += spr->Height + DefaultTextOffset;
                    }
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
                    if (Vertical)
                    {
                        textX += spr->Width + DefaultTextOffset;
                    }
                    else
                    {
                        textX += Length + DefaultTextOffset;
                    }
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
                    int sliderWidth = Length;
                    if (Vertical)
                    {
                        sliderWidth = spr->Width;
                    }
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
                    int sliderHeight = Length;
                    if (!Vertical)
                    {
                        sliderHeight = spr->Height;
                    }
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

CSize CGUISlider::GetSize()
{
    CSize size;
    auto spr = g_Game.ExecuteGump(Graphic);
    if (spr != nullptr)
    {
        if (Vertical)
        {
            size.Width = spr->Width;
            size.Height = Length;
        }
        else
        {
            size.Width = Length;
            size.Height = spr->Height;
        }
        if (HaveText)
        {
            //Text.Draw(TextX, TextY, checktrans);
        }
    }
    return size;
}

void CGUISlider::OnScroll(bool up, int delay)
{
    if (LastScrollTime < g_Ticks)
    {
        if (up)
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

        LastScrollTime = g_Ticks + delay;

        CalculateOffset();
        UpdateText();
    }
}

void CGUISlider::OnClick(int x, int y)
{
    int length = Length;
    const int maxValue = MaxValue - MinValue;
    auto spr = g_Game.ExecuteGump(Graphic);
    if (spr != nullptr)
    {
        length -= (Vertical ? (spr->Height / 2) : spr->Width);
    }
    const float percents = ((Vertical ? y : x) / (float)length) * 100.0f;
    Value = (int)((maxValue * percents) / 100.0f) + MinValue;
    CalculateOffset();
    UpdateText();
}

void CGUISlider::CalculateOffset()
{
    if (Value < MinValue)
    {
        Value = MinValue;
    }
    else if (Value > MaxValue)
    {
        Value = MaxValue;
    }

    int value = Value - MinValue;
    int maxValue = MaxValue - MinValue;
    int length = Length;
    auto spr = g_Game.ExecuteGump(Graphic);
    if (spr != nullptr)
    {
        length -= (Vertical ? spr->Height : spr->Width);
    }

    if (maxValue > 0)
    {
        Percents = ((value / (float)maxValue) * 100.0f);
    }
    else
    {
        Percents = 0.0f;
    }

    Offset = (int)((length * Percents) / 100.0f);
    if (Offset < 0)
    {
        Offset = 0;
    }
}

void CGUISlider::SetTextParameters(
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

void CGUISlider::PrepareTextures()
{
    g_Game.ExecuteGump(Graphic);
    g_Game.ExecuteGump(GraphicSelected);
    g_Game.ExecuteGump(GraphicPressed);
    if (BackgroundGraphic != 0u)
    {
        if (CompositeBackground)
        {
            g_Game.ExecuteGumpPart(BackgroundGraphic, 3);
        }
        else
        {
            g_Game.ExecuteGump(BackgroundGraphic);
        }
    }
}

uint16_t CGUISlider::GetDrawGraphic()
{
    uint16_t graphic = Graphic;
    if (g_GumpPressedElement == this)
    {
        graphic = GraphicPressed;
    }
    else if (g_GumpSelectedElement == this)
    {
        graphic = GraphicSelected;
    }
    return graphic;
}

void CGUISlider::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);

    /*Value++;
	if (Value > MaxValue)
	{
		Value = MinValue;

		TextPosition = (SLIDER_TEXT_POSITION)(TextPosition + 1);

		if (TextPosition > STP_RIGHT)
			TextPosition = STP_TOP;
	}
	UpdateText();
	CalculateOffset();*/

#ifndef NEW_RENDERER_ENABLED
    glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
#else
    ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
    cmd.value.asInt1 = SDM_NO_COLOR;
    RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif
    if (BackgroundGraphic != 0u)
    {
        if (CompositeBackground)
        {
            CGLTexture *th[3] = { nullptr };
            for (int i = 0; i < 3; i++)
            {
                auto spr = g_Game.ExecuteGump(BackgroundGraphic + (int)i);
                if (spr == nullptr || spr->Texture == nullptr)
                {
                    return;
                }
                th[i] = spr->Texture;
            }

            if (Vertical)
            {
                th[0]->Draw(m_X, m_Y, checktrans);
                th[2]->Draw(m_X, m_Y + (Length - th[2]->Height), checktrans);
                th[1]->Draw(
                    m_X,
                    m_Y + th[0]->Height,
                    0,
                    (Length - (th[0]->Height + th[2]->Height)),
                    checktrans);
            }
            else
            {
                th[0]->Draw(m_X, m_Y, checktrans);
                th[2]->Draw(m_X + (Length - th[2]->Width), m_Y, checktrans);
                th[1]->Draw(
                    m_X + th[0]->Width,
                    m_Y,
                    (Length - (th[0]->Width + th[2]->Width)),
                    0,
                    checktrans);
            }
        }
        else
        {
            auto spr = g_Game.ExecuteGump(BackgroundGraphic);
            if (spr != nullptr && spr->Texture != nullptr)
            {
                if (Vertical)
                {
                    spr->Texture->Draw(m_X, m_Y, 0, Length, checktrans);
                }
                else
                {
                    spr->Texture->Draw(m_X, m_Y, Length, 0, checktrans);
                }
            }
        }
    }

    auto spr = g_Game.ExecuteGump(GetDrawGraphic());
    if (spr != nullptr && spr->Texture != nullptr)
    {
        if (Vertical)
        {
            spr->Texture->Draw(m_X, m_Y + Offset, checktrans);
        }
        else
        {
            spr->Texture->Draw(m_X + Offset, m_Y, checktrans);
        }

        if (HaveText)
        {
            Text.Draw(TextX, TextY, checktrans);
        }
    }
}

bool CGUISlider::Select()
{
    auto spr = g_Game.ExecuteGump(Graphic);
    if (spr != nullptr)
    {
        int buttonX = m_X;
        int buttonY = m_Y;
        if (Vertical)
        {
            buttonY += Offset;
        }
        else
        {
            buttonX += Offset;
        }
        if (spr->Select(buttonX, buttonY, !BoundingBoxCheck))
        {
            return true;
        }

        if (BackgroundGraphic != 0u)
        {
            const int x = g_MouseManager.Position.X - m_X;
            const int y = g_MouseManager.Position.Y - m_Y;
            if (x >= 0 && y >= 0)
            {
                if (Vertical)
                {
                    return (x < spr->Width && y < Length);
                }
                {
                    return (x < Length && y < spr->Height);
                }
            }
        }
    }
    return false;
}

void CGUISlider::OnMouseEnter()
{
    if (g_SelectedObject.Gump != nullptr)
    {
        g_SelectedObject.Gump->WantRedraw = true;
    }
}

void CGUISlider::OnMouseExit()
{
    if (g_LastSelectedObject.Gump != nullptr)
    {
        g_LastSelectedObject.Gump->WantRedraw = true;
    }
}

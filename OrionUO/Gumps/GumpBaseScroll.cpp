// MIT License
// Copyright (C) August 2016 Hotride

#include "GumpBaseScroll.h"
#include "../OrionUO.h"
#include "../Managers/MouseManager.h"

CGumpBaseScroll::CGumpBaseScroll(
    GUMP_TYPE type,
    uint32_t serial,
    uint16_t graphic,
    int height,
    short x,
    short y,
    bool haveMinimizer,
    int scrollerOffsetY,
    bool haveBackgroundLines,
    int scissorOffsetHeight)
    : CGump(type, serial, x, y)
    , Height(height)
    , ScrollerOffsetY(scrollerOffsetY)
    , HaveBackgroundLines(haveBackgroundLines)
    , ScissorOffsetHeight(scissorOffsetHeight)
{
    DEBUG_TRACE_FUNCTION;
    Page = 2;
    Add(new CGUIPage(2));

    int offsetY = 0;

    if (haveMinimizer)
    {
        CGLTexture *th = g_Orion.ExecuteGump(0x082D);

        if (th != nullptr)
        {
            offsetY = th->Height - 3;
        }
    }

    m_Minimizer =
        (CGUIButton *)Add(new CGUIButton(ID_GBS_BUTTON_MINIMIZE, 0x082D, 0x082D, 0x082D, 0, 0));
    m_Minimizer->Visible = haveMinimizer;

    m_Background =
        (CGUIScrollBackground *)Add(new CGUIScrollBackground(0, graphic, 0, offsetY, Height));
    CRect rect = m_Background->WorkSpace;

    if (type != GT_SKILLS)
    {
        m_Minimizer->SetX(137);
    }
    else
    {
        m_Minimizer->SetX(170);
    }

    int heightDecBonus = ScissorOffsetHeight;

    if (HaveBackgroundLines)
    {
        ScrollerOffsetY += 16;
        heightDecBonus += 16;
    }

    m_HTMLGump = (CGUIHTMLGump *)Add(new CGUIHTMLGump(
        ID_GBS_HTMLGUMP,
        0,
        rect.Position.X,
        offsetY + rect.Position.Y + ScrollerOffsetY,
        rect.Size.Width,
        rect.Size.Height - ScrollerOffsetY - heightDecBonus,
        false,
        true));

    CGUIHTMLButton *button = m_HTMLGump->m_ButtonUp;

    if (button != nullptr)
    {
        button->Graphic = 0x0824;
        button->GraphicSelected = 0x0824;
        button->GraphicPressed = 0x0824;
        button->CheckPolygone = true;
    }

    button = m_HTMLGump->m_ButtonDown;

    if (button != nullptr)
    {
        button->Graphic = 0x0825;
        button->GraphicSelected = 0x0825;
        button->GraphicPressed = 0x0825;
        button->CheckPolygone = true;
    }

    CGUIHTMLSlider *slider = m_HTMLGump->m_Slider;

    if (slider != nullptr)
    {
        slider->Graphic = 0x001F;
        slider->GraphicSelected = 0x001F;
        slider->GraphicPressed = 0x001F;
        slider->BackgroundGraphic = 0;
    }

    m_Resizer = (CGUIResizeButton *)Add(new CGUIResizeButton(
        ID_GBS_BUTTON_RESIZE, 0x082E, 0x082F, 0x082F, 0, offsetY + Height - 3));

    if (type != GT_SKILLS)
    {
        m_Resizer->SetX(137);
    }
    else
    {
        m_Resizer->SetX(170);
    }
}

CGumpBaseScroll::~CGumpBaseScroll()
{
}

void CGumpBaseScroll::UpdateHeight()
{
    DEBUG_TRACE_FUNCTION;
    Height = StartResizeHeight + g_MouseManager.LeftDroppedOffset().Y;

    if (Height < m_MinHeight)
    {
        Height = m_MinHeight;
    }

    int maxHeight;
    GetDisplaySize(nullptr, &maxHeight);
    maxHeight -= 50;

    if (Height >= maxHeight)
    {
        Height = maxHeight;
    }

    int offsetY = 0;

    CGLTexture *th = g_Orion.ExecuteGump(0x082D);

    if (th != nullptr && m_Minimizer->Visible)
    {
        offsetY = th->Height - 3;
    }

    m_Background->UpdateHeight(Height);

    int heightDecBonus = ScissorOffsetHeight;

    if (HaveBackgroundLines)
    {
        heightDecBonus += 16;
    }

    m_HTMLGump->UpdateHeight(
        m_Background->WorkSpace.Size.Height - ScrollerOffsetY - heightDecBonus);

    m_Resizer->SetY(offsetY + Height - 3);
}

void CGumpBaseScroll::GUMP_RESIZE_START_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    StartResizeHeight = Height;
}

void CGumpBaseScroll::GUMP_RESIZE_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    if (StartResizeHeight != 0)
    {
        UpdateHeight();
        RecalculateSize();
    }
}

void CGumpBaseScroll::GUMP_RESIZE_END_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    if (StartResizeHeight != 0)
    {
        StartResizeHeight = 0;
    }
}

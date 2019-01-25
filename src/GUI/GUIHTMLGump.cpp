// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIHTMLGump.h"
#include "GUIHTMLResizepic.h"
#include "GUIHTMLHitBox.h"
#include "GUIHTMLSlider.h"
#include "GUIHTMLButton.h"
#include "GUIScissor.h"
#include "../CrossUO.h"
#include "../Point.h"
#include "../Managers/MouseManager.h"

CGUIHTMLGump::CGUIHTMLGump(
    int serial,
    uint16_t graphic,
    int x,
    int y,
    int width,
    int height,
    bool haveBackground,
    bool haveScrollbar)
    : CGUIPolygonal(GOT_HTMLGUMP, x, y, width, height)
    , HaveBackground(haveBackground)
    , HaveScrollbar(haveScrollbar)

{
    DEBUG_TRACE_FUNCTION;
    Serial = serial;
    Graphic = graphic;

    Initalize();
}

CGUIHTMLGump::~CGUIHTMLGump()
{
}

void CGUIHTMLGump::PrepareTextures()
{
    DEBUG_TRACE_FUNCTION;
    QFOR(item, m_Items, CBaseGUI *)
    item->PrepareTextures();
}

void CGUIHTMLGump::Initalize(bool menu)
{
    DEBUG_TRACE_FUNCTION;
    Clear();

    if (menu)
    {
        m_Background = new CGUIHTMLResizepic(this, 0, 0, 0, 0, 1, 1);
        m_Background->Visible = false;

        m_HitBoxLeft = new CGUIHTMLHitBox(this, Serial, -15, -2, 12, 52);

        m_HitBoxRight = new CGUIHTMLHitBox(this, Serial, Width + 3, -2, 12, 52);
        m_HitBoxRight->Color = 1;

        m_Slider = new CGUIHTMLSlider(
            this,
            Serial,
            0x00D8,
            0x00D8,
            0x00D8,
            0x00D5,
            true,
            false,
            -10,
            Height + 1,
            Width + 20,
            0,
            0,
            0);
        m_Slider->ScrollStep = GUMP_MENU_PIXEL_STEP;
        m_Slider->Vertical = false;

        m_Scissor = new CGUIScissor(true, m_X, m_Y, 0, 0, Width, Height);

        Add(m_Background);
        Add(m_Slider);
        Add(m_HitBoxLeft);
        Add(m_HitBoxRight);
        Add(m_Scissor);
    }
    else
    {
        int offsetWidth = Width;

        if (HaveScrollbar)
        {
            CGLTexture *th = g_Game.ExecuteGump(0x00FE);

            if (th != nullptr)
            {
                offsetWidth -= th->Width;
            }
        }

        m_Background = new CGUIHTMLResizepic(this, 0, Graphic, 0, 0, offsetWidth, Height);
        m_Background->Visible = HaveBackground;

        m_ButtonUp = new CGUIHTMLButton(this, Serial, 0x00FA, 0x00FA, 0x00FA, offsetWidth, 0);
        m_ButtonUp->Visible = HaveScrollbar;

        CGLTexture *thDown = g_Game.ExecuteGump(0x00FC);

        int sliderHeight = Height;

        if (thDown != nullptr)
        {
            sliderHeight -= thDown->Height;
        }

        m_ButtonDown =
            new CGUIHTMLButton(this, Serial, 0x00FC, 0x00FC, 0x00FC, offsetWidth, sliderHeight);
        m_ButtonDown->Color = 1;
        m_ButtonDown->Visible = HaveScrollbar;

        CGLTexture *thUp = g_Game.ExecuteGump(0x00FA);

        int sliderStartY = 0;

        if (thUp != nullptr)
        {
            sliderStartY = thUp->Height;
            sliderHeight -= sliderStartY;
        }

        m_Slider = new CGUIHTMLSlider(
            this,
            Serial,
            0x00FE,
            0x00FE,
            0x00FE,
            0x0100,
            false,
            true,
            offsetWidth,
            sliderStartY,
            sliderHeight,
            0,
            0,
            0);
        //m_Slider->SetTextParameters(true, STP_RIGHT_CENTER, 3, 0x0021, false);
        m_Slider->Visible = HaveScrollbar;

        m_Scissor = new CGUIScissor(true, m_X, m_Y, 0, 0, offsetWidth, Height);

        Add(m_Background);
        Add(m_Slider);
        Add(m_ButtonUp);
        Add(m_ButtonDown);
        Add(m_Scissor);
    }
}

void CGUIHTMLGump::UpdateHeight(int height)
{
    DEBUG_TRACE_FUNCTION;
    Height = height;

    m_Background->Height = height;

    CGLTexture *thDown = g_Game.ExecuteGump(m_ButtonDown->Graphic);

    int sliderHeight = height;

    if (thDown != nullptr)
    {
        sliderHeight -= thDown->Height;
    }

    m_ButtonDown->SetY(sliderHeight);

    CGLTexture *thUp = g_Game.ExecuteGump(m_ButtonUp->Graphic);

    int sliderStartY = 0;

    if (thUp != nullptr)
    {
        sliderStartY = thUp->Height;
        sliderHeight -= sliderStartY;
    }

    m_Slider->Length = sliderHeight;

    m_Scissor->Height = height;

    CalculateDataSize();
}

void CGUIHTMLGump::ResetDataOffset()
{
    DEBUG_TRACE_FUNCTION;
    m_Slider->Value = 0;
    CurrentOffset.Reset();
}

void CGUIHTMLGump::CalculateDataSize(CBaseGUI *item, int &startX, int &startY, int &endX, int &endY)
{
    DEBUG_TRACE_FUNCTION;
    for (; item != nullptr; item = (CBaseGUI *)item->m_Next)
    {
        if (item->Type == GOT_HITBOX || !item->Visible)
        {
            continue;
        }
        if (item->Type == GOT_DATABOX)
        {
            CalculateDataSize((CBaseGUI *)item->m_Items, startX, startY, endX, endY);
            continue;
        }

        if (item->GetX() < startX)
        {
            startX = item->GetX();
        }

        if (item->GetY() < startY)
        {
            startY = item->GetY();
        }

        CSize size = item->GetSize();

        int curX = item->GetX() + size.Width;
        int curY = item->GetY() + size.Height;

        if (curX > endX)
        {
            endX = curX;
        }

        if (curY > endY)
        {
            endY = curY;
        }
    }
}

void CGUIHTMLGump::CalculateDataSize()
{
    DEBUG_TRACE_FUNCTION;
    CBaseGUI *item = (CBaseGUI *)m_Items;

    for (int i = 0; i < 5; i++)
    {
        item = (CBaseGUI *)item->m_Next;
    }

    int startX = 0;
    int startY = 0;
    int endX = 0;
    int endY = 0;

    CalculateDataSize(item, startX, startY, endX, endY);

    DataSize.Width = abs(startX) + abs(endX);
    DataSize.Height = abs(startY) + abs(endY);

    DataOffset.X = startX;
    DataOffset.Y = startY;

    AvailableOffset.X = DataSize.Width - m_Scissor->Width;

    if (AvailableOffset.X < 0)
    {
        AvailableOffset.X = 0;
    }

    AvailableOffset.Y = DataSize.Height - m_Scissor->Height;

    if (AvailableOffset.Y < 0)
    {
        AvailableOffset.Y = 0;
    }

    m_Slider->MinValue = 0;

    if (m_Slider->Vertical)
    {
        m_Slider->MaxValue = AvailableOffset.Y;
    }
    else
    {
        m_Slider->MaxValue = AvailableOffset.X;
    }

    m_Slider->CalculateOffset();
}

bool CGUIHTMLGump::EntryPointerHere()
{
    DEBUG_TRACE_FUNCTION;
    QFOR(item, m_Items, CBaseGUI *)
    {
        if (item->Visible && item->EntryPointerHere())
        {
            return true;
        }
    }

    return false;
}

bool CGUIHTMLGump::Select()
{
    DEBUG_TRACE_FUNCTION;
    CPoint2Di oldPos = g_MouseManager.Position;
    g_MouseManager.Position = CPoint2Di(oldPos.X - m_X, oldPos.Y - m_Y);

    bool selected = false;

    CBaseGUI *item = (CBaseGUI *)m_Items;

    for (int i = 0; i < 5 && !selected; i++)
    {
        selected = item->Select();

        item = (CBaseGUI *)item->m_Next;
    }

    g_MouseManager.Position = oldPos;

    return selected;
}

void CGUIHTMLGump::Scroll(bool up, int delay)
{
    DEBUG_TRACE_FUNCTION;
    if (m_Slider != nullptr)
    {
        m_Slider->OnScroll(up, delay);
    }
}

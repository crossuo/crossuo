// MIT License
// Copyright (C) August 2016 Hotride

#include "GumpBuff.h"
#include "../Point.h"
#include "../OrionUO.h"
#include "../ToolTip.h"
#include "../SelectedObject.h"
#include "../Managers/FontsManager.h"
#include "../Managers/ConfigManager.h"

CGumpBuff::CGumpBuff(short x, short y)
    : CGump(GT_BUFF, 0, x, y)
{
    DEBUG_TRACE_FUNCTION;
    Graphic = 0x7580;
    m_Locker.Serial = ID_GB_LOCK_MOVING;

    Add(new CGUIGumppic(Graphic, 0, 0));
    Add(new CGUIHitBox(0, 0, 0, 0, 0));
    Add(new CGUIButton(ID_GB_NEXT_WINDOW_DIRECTION, 0x7585, 0x7589, 0x7589, 0, 0));
    Add(new CGUIAlphaBlending(true, 0.1f));
    Add(new CGUIAlphaBlending(false, 0.1f));
}

CGumpBuff::~CGumpBuff()
{
}

bool CGumpBuff::CanBeDisplayed()
{
    DEBUG_TRACE_FUNCTION;
    return g_ConfigManager.ToggleBufficonWindow;
}

void CGumpBuff::UpdateBuffIcons()
{
    DEBUG_TRACE_FUNCTION;
    for (CBaseGUI *item = (CBaseGUI *)m_Items; item != nullptr;)
    {
        CBaseGUI *next = (CBaseGUI *)item->m_Next;

        if (item->Type == GOT_BUFF)
        {
            CGUIBuff *buff = (CGUIBuff *)item;

            int delta = (int)(buff->Timer - g_Ticks);

            if (buff->Timer != 0xFFFFFFFF && delta < USE_ALPHA_BLENDING_WHEN_TIMER_LESS)
            {
                WantRedraw = true;

                if (delta <= 0)
                {
                    Delete(buff);
                    WantUpdateContent = true;
                }
                else
                {
                    int alpha = buff->Alpha;
                    int addVal = (USE_ALPHA_BLENDING_WHEN_TIMER_LESS - delta) / ALPHA_CHANGE_KOEFF;

                    if (buff->DecAlpha)
                    {
                        alpha -= addVal;

                        if (alpha <= MUNIMUM_ICON_ALPHA)
                        {
                            buff->DecAlpha = false;
                            alpha = MUNIMUM_ICON_ALPHA;
                        }
                    }
                    else
                    {
                        alpha += addVal;

                        if (alpha >= 255)
                        {
                            buff->DecAlpha = true;
                            alpha = 255;
                        }
                    }

                    buff->Alpha = alpha;
                }
            }
        }

        item = next;
    }
}

void CGumpBuff::AddBuff(uint16_t id, uint16_t timer, const wstring &text)
{
    DEBUG_TRACE_FUNCTION;
    uint32_t ticks = 0xFFFFFFFF;
    if (timer != 0u)
    {
        ticks = g_Ticks + (timer * 1000);
    }

    QFOR(item, m_Items, CBaseGUI *)
    {
        if (item->Type == GOT_BUFF && item->Graphic == id)
        {
            CGUIBuff *buff = (CGUIBuff *)item;

            buff->Timer = ticks;
            buff->Text = text;
            buff->DecAlpha = true;
            buff->Alpha = 0xFF;
            buff->TooltipTimer = 0;

            MoveToBack(buff);
            WantUpdateContent = true;

            return;
        }
    }

    Add(new CGUIBuff(id, ticks, text));
    WantUpdateContent = true;
}

void CGumpBuff::DeleteBuff(uint16_t id)
{
    DEBUG_TRACE_FUNCTION;
    QFOR(item, m_Items, CBaseGUI *)
    {
        if (item->Type == GOT_BUFF && item->Graphic == id)
        {
            Delete(item);
            WantUpdateContent = true;

            break;
        }
    }
}

void CGumpBuff::InitToolTip()
{
    DEBUG_TRACE_FUNCTION;
    if (g_SelectedObject.Serial == ID_GB_NEXT_WINDOW_DIRECTION)
    {
        g_ToolTip.Set(L"Change buff window gump");
    }
    else if (g_SelectedObject.Object != nullptr)
    {
        QFOR(item, m_Items, CBaseGUI *)
        {
            if (item != g_SelectedObject.Object)
            {
                continue;
            }

            if (item->Type == GOT_BUFF)
            {
                CGUIBuff *buff = (CGUIBuff *)item;

                g_FontManager.SetUseHTML(true);

                if (buff->Timer != 0xFFFFFFFF && buff->TooltipTimer < g_Ticks)
                {
                    buff->TooltipTimer = g_Ticks + ((buff->Timer - g_Ticks) % 1000);
                    g_ToolTip.Reset();

                    wchar_t buf[512] = { 0 };
                    swprintf(
                        buf,
                        sizeof(buf),
                        L"%s\nTimeLeft: %i seconds.",
                        buff->Text.c_str(),
                        (buff->Timer - g_Ticks) / 1000); // FIXME: check windows

                    g_ToolTip.Set(buf);

                    g_ToolTip.Timer = 0;
                    g_ToolTip.Use = true;
                }
                else
                {
                    g_ToolTip.Set(buff->Text);
                }

                g_FontManager.SetUseHTML(false);
            }
            else
            {
                g_ToolTip.Set(L"Buffs and Debuffs will appear here.", 120);
            }

            break;
        }
    }
}

void CGumpBuff::GetGumpStatus(
    CPoint2Di &ball,
    CPoint2Di &items,
    bool &useX,
    bool &decX,
    bool &decY,
    CPoint2Di &startGump,
    CSize &endGump)
{
    DEBUG_TRACE_FUNCTION;
    startGump.X = 0;
    startGump.Y = 0;

    endGump = g_Orion.GetGumpDimension(Graphic);

    switch (Graphic)
    {
        case 0x757F: //v
        {
            ball.X = 0;
            ball.Y = 0;
            items.X = 25;
            items.Y = 25;
            decY = false;
            decX = false;
            useX = false;
            break;
        }
        case 0x7581: //^
        {
            ball.X = 34;
            ball.Y = 78;
            items.X = 7;
            items.Y = 52;
            decY = true;
            decX = false;
            useX = false;
            break;
        }
        case 0x7582: //<
        {
            ball.X = 76;
            ball.Y = 36;
            items.X = 52;
            items.Y = 7;
            decY = false;
            decX = true;
            useX = true;
            break;
        }
        case 0x7580: //>
        default:
        {
            ball.X = -2;
            ball.Y = 36;
            items.X = 20;
            items.Y = 7;
            decY = false;
            decX = false;
            useX = true;
            break;
        }
    }

    CPoint2Di itemsOfs = items;

    QFOR(item, m_Items, CBaseGUI *)
    {
        if (item->Type != GOT_BUFF)
        {
            continue;
        }

        bool moved = false;

        CSize gumpDim = g_Orion.GetGumpDimension(item->Graphic);

        if (useX)
        {
            if (decX)
            {
                itemsOfs.X -= gumpDim.Width + BUFF_ITEM_STEP_OFFSET_X;
            }
            else
            {
                itemsOfs.X += gumpDim.Width + BUFF_ITEM_STEP_OFFSET_X;
            }
        }
        else
        {
            if (decY)
            {
                itemsOfs.Y -= gumpDim.Height + BUFF_ITEM_STEP_OFFSET_Y;
            }
            else
            {
                itemsOfs.Y += gumpDim.Height + BUFF_ITEM_STEP_OFFSET_Y;
            }
        }
    }

    if (useX)
    {
        if (decX)
        {
            itemsOfs.X -= 20;
        }
        else
        {
            itemsOfs.X += 20;
        }
    }
    else
    {
        if (decY)
        {
            itemsOfs.Y -= 20;
        }
        else
        {
            itemsOfs.Y += 20;
        }
    }

    if (itemsOfs.X < startGump.X)
    {
        startGump.X = itemsOfs.X;
    }

    if (itemsOfs.Y < startGump.Y)
    {
        startGump.Y = itemsOfs.Y;
    }

    if (itemsOfs.X > endGump.Width)
    {
        endGump.Width = itemsOfs.X;
    }

    if (itemsOfs.Y > endGump.Height)
    {
        endGump.Height = itemsOfs.Y;
    }
}

void CGumpBuff::PrepareContent()
{
    DEBUG_TRACE_FUNCTION;
    if (Graphic < 0x757F || Graphic > 0x7582)
    {
        Graphic = 0x7580;
        WantUpdateContent = true;
    }
}

void CGumpBuff::UpdateContent()
{
    DEBUG_TRACE_FUNCTION;
    bool decX = false;
    bool decY = false;
    bool useX = true;

    CPoint2Di ballCoordinates;
    CPoint2Di startCoordinates;
    CPoint2Di startGump;
    CSize endGump;

    GetGumpStatus(ballCoordinates, startCoordinates, useX, decX, decY, startGump, endGump);

    //Body
    CBaseGUI *gui = (CBaseGUI *)m_Items;
    gui->Graphic = Graphic;

    //Selection zone
    gui = (CBaseGUI *)gui->m_Next;
    gui->SetX(startGump.X);
    gui->SetY(startGump.Y);
    ((CGUIPolygonal *)gui)->Width = endGump.Width;
    ((CGUIPolygonal *)gui)->Height = endGump.Height;

    //Crystall ball
    gui = (CBaseGUI *)gui->m_Next;
    gui->SetX(ballCoordinates.X);
    gui->SetY(ballCoordinates.Y);

    gui = nullptr;

    QFOR(item, m_Items, CBaseGUI *)
    {
        if (item->Type != GOT_BUFF)
        {
            if (item->Type == GOT_BLENDING && !item->Enabled)
            {
                gui = item;
            }

            continue;
        }

        CGUIBuff *buff = (CGUIBuff *)item;

        CSize gumpDim = g_Orion.GetGumpDimension(buff->Graphic);
        buff->SetX(startCoordinates.X);
        buff->SetY(startCoordinates.Y);

        if (useX)
        {
            if (decX)
            {
                startCoordinates.X -= gumpDim.Width + BUFF_ITEM_STEP_OFFSET_X;
            }
            else
            {
                startCoordinates.X += gumpDim.Width + BUFF_ITEM_STEP_OFFSET_X;
            }
        }
        else
        {
            if (decY)
            {
                startCoordinates.Y -= gumpDim.Height + BUFF_ITEM_STEP_OFFSET_Y;
            }
            else
            {
                startCoordinates.Y += gumpDim.Height + BUFF_ITEM_STEP_OFFSET_Y;
            }
        }
    }

    //Выключение прозрачности в задницу очереди
    if (gui != nullptr)
    {
        MoveToBack(gui);
    }
}

void CGumpBuff::GUMP_BUTTON_EVENT_C
{
    DEBUG_TRACE_FUNCTION;
    if (serial == ID_GB_NEXT_WINDOW_DIRECTION)
    {
        switch (Graphic)
        {
            case 0x7580:
            {
                Graphic = 0x7582;
                break;
            }
            case 0x7581:
            {
                Graphic = 0x757F;
                break;
            }
            case 0x7582:
            {
                Graphic = 0x7581;
                break;
            }
            case 0x757F:
            default:
            {
                Graphic = 0x7580;
                break;
            }
        }

        WantUpdateContent = true;
    }
    else if (serial == ID_GB_LOCK_MOVING)
    {
        LockMoving = !LockMoving;
    }
}

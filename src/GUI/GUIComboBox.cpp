// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIComboBox.h"
#include "GUIText.h"
#include "../Point.h"
#include "../CrossUO.h"
#include "../PressedObject.h"
#include "../SelectedObject.h"
#include "../Managers/MouseManager.h"

CGUIComboBox::CGUIComboBox(
    int serial,
    uint16_t graphic,
    bool compositeBackground,
    uint16_t openGraphic,
    int x,
    int y,
    int width,
    int showItemsCount,
    bool showMaximizedCenter)
    : CBaseGUI(GOT_COMBOBOX, serial, graphic, 0, x, y)
    , OpenGraphic(openGraphic)
    , CompositeBackground(compositeBackground)
    , m_ShowItemsCount(showItemsCount)
    , Width(width)
    , OpenedWidth(width)
    , ShowMaximizedCenter(showMaximizedCenter)
{
    DEBUG_TRACE_FUNCTION;
    MoveOnDrag = false;
    m_ArrowX = 0;
    m_OffsetY = 0;
    m_StepY = 0;
    m_MinimizedArrowX = Width - 16;
    m_WorkWidth = Width - 6;
    m_WorkHeight = m_ShowItemsCount * 15;

    if (CompositeBackground)
    {
        CGLTexture *th = g_Game.ExecuteGump(OpenGraphic);

        if (th != nullptr)
        {
            m_ArrowX = th->Width - 24;
            m_OffsetY = th->Height;
        }

        th = g_Game.ExecuteGump(OpenGraphic + 1);

        if (th != nullptr)
        {
            m_StepY = th->Height;
            m_WorkWidth = th->Width - 12;
        }

        th = g_Game.ExecuteGump(Graphic);

        if (th != nullptr && (Width == 0))
        {
            m_MinimizedArrowX = th->Width - 16;
        }
    }
}

CGUIComboBox::~CGUIComboBox()
{
    DEBUG_TRACE_FUNCTION;
    if (Text != nullptr)
    {
        delete Text;
        Text = nullptr;
    }
}

void CGUIComboBox::RecalculateWidth()
{
    DEBUG_TRACE_FUNCTION;
    if (!CompositeBackground)
    {
        OpenedWidth = 0;

        QFOR(item, m_Items, CBaseGUI *)
        {
            if (item->Type == GOT_TEXT)
            {
                CGUIText *text = (CGUIText *)item;

                if (OpenedWidth < text->m_Texture.Width)
                {
                    OpenedWidth = text->m_Texture.Width;
                }
            }
        }

        OpenedWidth += 8;

        if (OpenedWidth < Width)
        {
            OpenedWidth = Width;
        }

        m_WorkWidth = OpenedWidth - 6;
    }
}

void CGUIComboBox::SetShowItemsCount(int val)
{
    DEBUG_TRACE_FUNCTION;
    m_WorkHeight = val * 15;
    m_ShowItemsCount = val;
}

CSize CGUIComboBox::GetSize()
{
    DEBUG_TRACE_FUNCTION;

    return CSize(m_WorkWidth, m_WorkHeight);
}

void CGUIComboBox::PrepareTextures()
{
    DEBUG_TRACE_FUNCTION;
    if (CompositeBackground)
    {
        g_Game.ExecuteGump(Graphic);
        g_Game.ExecuteGump(0x0985);
        g_Game.ExecuteGump(0x0983);
        g_Game.ExecuteGumpPart(OpenGraphic, 5);
    }
    else
    {
        g_Game.ExecuteResizepic(Graphic);
        g_Game.ExecuteResizepic(OpenGraphic);
        g_Game.ExecuteGump(0x00FC);
    }
}

CBaseGUI *CGUIComboBox::SkipToStart()
{
    DEBUG_TRACE_FUNCTION;
    CBaseGUI *start = (CBaseGUI *)m_Items;

    int index = 0;

    QFOR(item, m_Items, CBaseGUI *)
    {
        if (item->Type == GOT_TEXT)
        {
            if (index == StartIndex)
            {
                start = (CBaseGUI *)item;
                break;
            }

            index++;
        }
    }

    return start;
}

void CGUIComboBox::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    if (Text != nullptr)
    {
        Text->m_Texture.Draw(m_X + Text->GetX(), m_Y + Text->GetY() + TextOffsetY, checktrans);
    }

    if (g_PressedObject.LeftObject == this) //maximized
    {
        int currentX = m_X + 3;
        int currentY = m_Y + 3;

        if (ShowMaximizedCenter)
        {
            currentY -= m_WorkHeight / 2;
        }

        if (CompositeBackground)
        {
            int bodyY = m_Y + m_OffsetY;
            int bodyStep = m_StepY;

            currentX = m_X + 12;
            currentY = bodyY + 1;
            int posY = m_Y;

            if (ShowMaximizedCenter)
            {
                currentY -= m_WorkHeight / 2;
                posY -= m_WorkHeight / 2;
                bodyY -= m_WorkHeight / 2;
            }

            g_Game.DrawGump(OpenGraphic, 0, m_X, posY);
            g_Game.DrawGump(0x0983, 0, m_X + m_ArrowX, posY + 2);

            int graphicOffset = 0;

            for (int i = 0; i < m_ShowItemsCount; i++)
            {
                g_Game.DrawGump(OpenGraphic + 1 + graphicOffset, 0, m_X + 5, bodyY);
                graphicOffset = (graphicOffset + 1) % 3;
                bodyY += bodyStep;
            }

            g_Game.DrawGump(OpenGraphic + 4, 0, m_X, bodyY);
            g_Game.DrawGump(0x0985, 0, m_X + m_ArrowX, bodyY);
        }
        else
        {
            g_Game.DrawResizepicGump(OpenGraphic, m_X, m_Y, OpenedWidth, m_WorkHeight + 6);
        }

        g_GL.PushScissor(currentX, currentY, m_WorkWidth, m_WorkHeight);

        CBaseGUI *start = SkipToStart();
        int count = 0;

        QFOR(item, start, CBaseGUI *)
        {
            if (item->Type == GOT_TEXT)
            {
                if (g_SelectedObject.Object == item)
                {
                    glColor4f(0.7f, 0.7f, 0.7f, 1.0f);
                    g_GL.DrawPolygone(currentX, currentY, m_WorkWidth, 14);
                    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
                }

                CGUIText *text = (CGUIText *)item;

                text->m_Texture.Draw(currentX, currentY + TextOffsetY);
                currentY += 15;

                count++;

                if (count > m_ShowItemsCount)
                {
                    break;
                }
            }
        }

        g_GL.PopScissor();
    }
    else
    {
        CGUIText *selected = nullptr;
        int index = 0;

        QFOR(item, m_Items, CBaseGUI *)
        {
            if (item->Type == GOT_TEXT)
            {
                if (index == SelectedIndex)
                {
                    selected = (CGUIText *)item;
                    break;
                }

                index++;
            }
        }

        if (CompositeBackground)
        {
            if (Width != 0)
            {
                g_Game.DrawGump(Graphic, 0, m_X, m_Y, Width, 0);
            }
            else
            {
                g_Game.DrawGump(Graphic, 0, m_X, m_Y);
            }

            if (selected != nullptr)
            {
                g_GL.PushScissor(m_X + 6, m_Y, m_MinimizedArrowX, 20);
                selected->m_Texture.Draw(m_X + 6, m_Y + 6 + TextOffsetY);
                g_GL.PopScissor();
            }

            g_Game.DrawGump(0x0985, 0, m_X + m_MinimizedArrowX, m_Y + 6);
        }
        else
        {
            g_Game.DrawResizepicGump(Graphic, m_X, m_Y, Width, 20);

            if (selected != nullptr)
            {
                g_GL.PushScissor(m_X + 3, m_Y, Width - 6, 20);
                selected->m_Texture.Draw(m_X + 3, m_Y + 4 + TextOffsetY);
                g_GL.PopScissor();
            }

            g_Game.DrawGump(0x00FC, 0, m_X + m_MinimizedArrowX, m_Y - 1);
        }
    }
}

bool CGUIComboBox::Select()
{
    DEBUG_TRACE_FUNCTION;
    ListingDirection = 0;
    bool select = false;

    if (g_PressedObject.LeftObject == this) //maximized
    {
        int currentX = m_X + 3;
        int currentY = m_Y + 3;

        if (CompositeBackground)
        {
            currentY = m_Y + m_OffsetY + 1;
            currentX = m_X + 12;
        }

        if (ShowMaximizedCenter)
        {
            currentY -= m_WorkHeight / 2;
        }

        select = g_Game.PolygonePixelsInXY(currentX, currentY, m_WorkWidth, m_WorkHeight);

        if (!select)
        {
            if (g_MouseManager.Position.Y < currentY)
            {
                ListingDirection = 1;
            }
            else if (g_MouseManager.Position.Y > currentY + m_WorkHeight)
            {
                ListingDirection = 2;
            }
        }
    }
    else
    {
        if (CompositeBackground)
        {
            if (Width != 0)
            {
                select = g_Game.GumpPixelsInXY(Graphic, m_X, m_Y, Width, 0);
            }
            else
            {
                select = g_Game.GumpPixelsInXY(Graphic, m_X, m_Y);
            }
        }
        else
        {
            select = g_Game.ResizepicPixelsInXY(Graphic, m_X, m_Y, Width, 20);
        }
    }

    return select;
}

CBaseGUI *CGUIComboBox::SelectedItem()
{
    DEBUG_TRACE_FUNCTION;
    CBaseGUI *select = this;

    if (g_PressedObject.LeftObject == this) //maximized
    {
        int currentX = m_X + 3;
        int currentY = m_Y + 3;

        if (CompositeBackground)
        {
            currentY = m_Y + m_OffsetY + 1;
            currentX = m_X + 12;
        }

        if (ShowMaximizedCenter)
        {
            currentY -= m_WorkHeight / 2;
        }

        CBaseGUI *start = SkipToStart();
        int count = 0;

        QFOR(item, start, CBaseGUI *)
        {
            if (item->Type == GOT_TEXT)
            {
                if (g_Game.PolygonePixelsInXY(currentX, currentY, m_WorkWidth, 14))
                {
                    select = item;
                    break;
                }

                currentY += 15;

                count++;

                if (count > m_ShowItemsCount)
                {
                    break;
                }
            }
        }
    }

    return select;
}

int CGUIComboBox::IsSelectedItem()
{
    DEBUG_TRACE_FUNCTION;
    int select = -1;

    if (g_PressedObject.LeftObject == this) //maximized
    {
        CBaseGUI *start = SkipToStart();
        int count = 0;

        QFOR(item, start, CBaseGUI *)
        {
            if (item->Type == GOT_TEXT)
            {
                if (g_SelectedObject.Object == item)
                {
                    select = StartIndex + count;
                    break;
                }

                count++;

                if (count > m_ShowItemsCount)
                {
                    break;
                }
            }
        }
    }

    return select;
}

// MIT License
// Copyright (C) August 2016 Hotride

#include "GUISkillGroup.h"
#include "GUIButton.h"
#include "GUITextEntry.h"
#include "GUISkillItem.h"
#include "../OrionUO.h"
#include "../Point.h"
#include "../SkillGroup.h"
#include "../Managers/MouseManager.h"

CGUISkillGroup::CGUISkillGroup(
    int serial, int minimizeSerial, CSkillGroupObject *group, int x, int y)
    : CBaseGUI(GOT_SKILLGROUP, serial, 0, 0, x, y)
{
    DEBUG_TRACE_FUNCTION;
    const bool isMinimized = !group->Maximized;
    const uint16_t graphic = (isMinimized ? 0x0827 : 0x0826);
    m_Minimizer = new CGUIButton(minimizeSerial, graphic, graphic, graphic, 0, 0);
    SetMinimized(isMinimized);

    m_Name = new CGUITextEntry(serial, 0, 0, 0, 16, -5, 0, false, 6);
    m_Name->m_Entry.SetTextA(group->Name);
}

CGUISkillGroup::~CGUISkillGroup()
{
    DEBUG_TRACE_FUNCTION;
    RELEASE_POINTER(m_Minimizer);
    RELEASE_POINTER(m_Name);
}

void CGUISkillGroup::SetMinimized(bool val)
{
    assert(m_Minimizer);

    DEBUG_TRACE_FUNCTION;
    m_Minimized = val;
    const uint16_t graphic = (val ? 0x0827 : 0x0826);
    m_Minimizer->Graphic = graphic;
    m_Minimizer->GraphicSelected = graphic;
    m_Minimizer->GraphicPressed = graphic;
}

void CGUISkillGroup::UpdateDataPositions()
{
    DEBUG_TRACE_FUNCTION;
    int y = 0;

    QFOR(item, m_Items, CBaseGUI *)
    {
        item->SetY(y);
        y += 17;
    }
}

void CGUISkillGroup::PrepareTextures()
{
    DEBUG_TRACE_FUNCTION;
    m_Minimizer->PrepareTextures();
    g_Orion.ExecuteGump(0x0835);
    m_Name->PrepareTextures();

    QFOR(item, m_Items, CBaseGUI *)
    item->PrepareTextures();
}

bool CGUISkillGroup::EntryPointerHere()
{
    DEBUG_TRACE_FUNCTION;
    return (g_EntryPointer == &m_Name->m_Entry);
}

CBaseGUI *CGUISkillGroup::SelectedItem()
{
    DEBUG_TRACE_FUNCTION;
    CBaseGUI *selected = m_Name;

    if (g_Orion.PolygonePixelsInXY(m_X + m_Minimizer->GetX(), m_Y + m_Minimizer->GetY(), 14, 14))
    {
        selected = m_Minimizer;
    }
    else if (!GetMinimized())
    {
        CPoint2Di oldMouse = g_MouseManager.Position;
        g_MouseManager.Position = CPoint2Di(oldMouse.X - m_X, oldMouse.Y - (m_Y + 19));

        QFOR(item, m_Items, CBaseGUI *)
        {
            if (item->Select())
            {
                selected = item;

                if (item->Type == GOT_SKILLITEM)
                {
                    selected = ((CGUISkillItem *)item)->SelectedItem();
                }

                break;
            }
        }

        g_MouseManager.Position = oldMouse;
    }

    return selected;
}

CSize CGUISkillGroup::GetSize()
{
    DEBUG_TRACE_FUNCTION;
    CSize size(220, 19);

    if (!GetMinimized() && m_Items != nullptr)
    {
        size.Height += GetItemsCount() * 17;
    }

    return size;
}

void CGUISkillGroup::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    glTranslatef((GLfloat)m_X, (GLfloat)m_Y, 0.0f);

    m_Minimizer->Draw(checktrans);

    bool drawOrnament = true;

    if (m_Name->Focused && g_EntryPointer == &m_Name->m_Entry)
    {
        drawOrnament = false;
        g_GL.DrawPolygone(16, 0, 200, 14);
    }
    else if (m_Name->Focused)
    {
        g_GL.DrawPolygone(16, 0, m_Name->m_Entry.m_Texture.Width, 14);
    }

    m_Name->Draw(checktrans);

    if (drawOrnament)
    {
        int x = 11 + m_Name->m_Entry.m_Texture.Width;
        int width = 215 - x;

        if (x > 0)
        {
            g_Orion.DrawGump(0x0835, 0, x, 5, width, 0);
        }
    }

    if (!GetMinimized() && m_Items != nullptr)
    {
        glTranslatef(0.0f, 19.0f, 0.0f);

        QFOR(item, m_Items, CBaseGUI *)
        item->Draw(checktrans);

        glTranslatef(0.0f, -19.0f, 0.0f);
    }

    glTranslatef((GLfloat)-m_X, (GLfloat)-m_Y, 0.0f);
}

bool CGUISkillGroup::Select()
{
    DEBUG_TRACE_FUNCTION;
    int x = g_MouseManager.Position.X - m_X;
    int y = g_MouseManager.Position.Y - m_Y;

    bool result = (x >= 0 && y >= 0 && x < 220 && y < 19);

    if (!GetMinimized() && !result)
    {
        CPoint2Di oldMouse = g_MouseManager.Position;
        g_MouseManager.Position = CPoint2Di(oldMouse.X - m_X, oldMouse.Y - (m_Y + 19));

        QFOR(item, m_Items, CBaseGUI *)
        {
            if (item->Select())
            {
                result = true;
                break;
            }
        }

        g_MouseManager.Position = oldMouse;
    }

    return result;
}

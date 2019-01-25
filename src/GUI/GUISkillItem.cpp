// MIT License
// Copyright (C) August 2016 Hotride

#include "GUISkillItem.h"
#include "GUIButton.h"
#include "../CrossUO.h"
#include "../PressedObject.h"
#include "../Managers/MouseManager.h"
#include "../Managers/SkillsManager.h"
#include "../Managers/FontsManager.h"

CGUISkillItem::CGUISkillItem(int serial, int useSerial, int statusSerial, int index, int x, int y)
    : CBaseGUI(GOT_SKILLITEM, serial, 0, 0, x, y)
    , Index(index)
{
    DEBUG_TRACE_FUNCTION;
    CSkill *skill = g_SkillsManager.Get(Index);

    if (skill != nullptr)
    {
        if (static_cast<int>(skill->Button) != 0)
        {
            m_ButtonUse = new CGUIButton(useSerial, 0x0837, 0x0838, 0x0838, 8, 0);
        }
        else
        {
            m_ButtonUse = nullptr;
        }

        m_Status = skill->Status;

        uint16_t graphic = GetStatusButtonGraphic();
        m_ButtonStatus = new CGUIButton(statusSerial, graphic, graphic, graphic, 251, 0);
        m_ButtonStatus->CheckPolygone = true;

        g_FontManager.GenerateA(9, m_NameText, skill->Name, 0x0288);

        CreateValueText();
    }
}

CGUISkillItem::~CGUISkillItem()
{
    DEBUG_TRACE_FUNCTION;
    m_NameText.Clear();
    m_ValueText.Clear();

    RELEASE_POINTER(m_ButtonUse);
    RELEASE_POINTER(m_ButtonStatus);
}

void CGUISkillItem::SetStatus(uint8_t val)
{
    DEBUG_TRACE_FUNCTION;
    m_Status = val;

    uint16_t graphic = GetStatusButtonGraphic();

    m_ButtonStatus->Graphic = graphic;
    m_ButtonStatus->GraphicSelected = graphic;
    m_ButtonStatus->GraphicPressed = graphic;
}

uint16_t CGUISkillItem::GetStatusButtonGraphic()
{
    DEBUG_TRACE_FUNCTION;
    uint16_t graphic = 0x0984; //Up

    if (m_Status == 1)
    {
        graphic = 0x0986; //Down
    }
    else if (m_Status == 2)
    {
        graphic = 0x082C; //Lock
    }

    return graphic;
}

void CGUISkillItem::CreateValueText(bool showReal, bool showCap)
{
    DEBUG_TRACE_FUNCTION;
    CSkill *skill = g_SkillsManager.Get(Index);

    if (skill != nullptr)
    {
        //Значение скилла (учитывая выбранный флаг отображения)
        float val = skill->BaseValue;
        if (showReal)
        {
            val = skill->Value;
        }
        else if (showCap)
        {
            val = skill->Cap;
        }

        char sbf[10] = { 0 };
        sprintf_s(sbf, "%.1f", val);

        g_FontManager.GenerateA(9, m_ValueText, sbf, 0x0288);
    }
}

void CGUISkillItem::PrepareTextures()
{
    DEBUG_TRACE_FUNCTION;
    if (m_ButtonUse != nullptr)
    {
        m_ButtonUse->PrepareTextures();
    }

    m_ButtonStatus->PrepareTextures();
}

CBaseGUI *CGUISkillItem::SelectedItem()
{
    DEBUG_TRACE_FUNCTION;
    CBaseGUI *selected = this;

    if (g_Game.PolygonePixelsInXY(
            m_X + m_ButtonStatus->GetX(), m_Y + m_ButtonStatus->GetY(), 14, 14))
    {
        selected = m_ButtonStatus;
    }
    else if (m_ButtonUse != nullptr)
    {
        if (g_Game.GumpPixelsInXY(
                m_ButtonUse->Graphic, m_X + m_ButtonUse->GetX(), m_Y + m_ButtonUse->GetY()))
        {
            selected = m_ButtonUse;
        }
    }

    return selected;
}

void CGUISkillItem::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    glTranslatef((GLfloat)m_X, (GLfloat)m_Y, 0.0f);

    if (m_ButtonUse != nullptr)
    {
        m_ButtonUse->Draw(checktrans);
    }

    if (g_PressedObject.LeftObject == this)
    {
        g_GL.DrawPolygone(20, 0, 250, 14);
    }

    m_NameText.Draw(22, 0, checktrans);

    m_ButtonStatus->Draw(checktrans);

    m_ValueText.Draw(250 - m_ValueText.Width, 0);

    glTranslatef((GLfloat)-m_X, (GLfloat)-m_Y, 0.0f);
}

bool CGUISkillItem::Select()
{
    DEBUG_TRACE_FUNCTION;
    int x = g_MouseManager.Position.X - m_X;
    int y = g_MouseManager.Position.Y - m_Y;

    return (x >= 0 && y >= 0 && x < 255 && y < 17);
}

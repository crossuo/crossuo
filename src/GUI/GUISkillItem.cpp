// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#include "GUISkillItem.h"
#include "../Platform.h"
#include "GUIButton.h"
#include "../CrossUO.h"
#include "../PressedObject.h"
#include "../Managers/MouseManager.h"
#include "../Managers/SkillsManager.h"
#include "../Managers/FontsManager.h"
#include "../Renderer/RenderAPI.h"
#include "../Utility/PerfMarker.h"

CGUISkillItem::CGUISkillItem(int serial, int useSerial, int statusSerial, int index, int x, int y)
    : CBaseGUI(GOT_SKILLITEM, serial, 0, 0, x, y)
    , Index(index)
{
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

        m_Status = skill->LockStatus;
        uint16_t graphic = GetStatusButtonGraphic();
        m_ButtonStatus = new CGUIButton(statusSerial, graphic, graphic, graphic, 251, 0);
        m_ButtonStatus->BoundingBoxCheck = true;
        g_FontManager.GenerateA(9, m_NameText, skill->Name, 0x0288);
        CreateValueText();
    }
}

CGUISkillItem::~CGUISkillItem()
{
    m_NameText.Clear();
    m_ValueText.Clear();

    if (m_ButtonUse)
        delete m_ButtonUse;
    m_ButtonUse = nullptr;
    if (m_ButtonStatus)
        delete m_ButtonStatus;
    m_ButtonStatus = nullptr;
}

void CGUISkillItem::SetStatus(uint8_t val)
{
    m_Status = val;

    uint16_t graphic = GetStatusButtonGraphic();

    m_ButtonStatus->Graphic = graphic;
    m_ButtonStatus->GraphicSelected = graphic;
    m_ButtonStatus->GraphicPressed = graphic;
}

uint16_t CGUISkillItem::GetStatusButtonGraphic()
{
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
    CSkill *skill = g_SkillsManager.Get(Index);
    if (skill != nullptr)
    {
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
    if (m_ButtonUse != nullptr)
    {
        m_ButtonUse->PrepareTextures();
    }

    m_ButtonStatus->PrepareTextures();
}

CBaseGUI *CGUISkillItem::SelectedItem()
{
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
    ScopedPerfMarker(__FUNCTION__);

    RenderAdd_SetModelViewTranslation(
        g_renderCmdList, SetModelViewTranslationCmd{ { (float)m_X, (float)m_Y, 0.0f } });

    if (m_ButtonUse != nullptr)
    {
        m_ButtonUse->Draw(checktrans);
    }

    if (g_PressedObject.LeftObject == this)
    {
        RenderAdd_DrawUntexturedQuad(g_renderCmdList, DrawUntexturedQuadCmd{ 20, 0, 250, 14 });
    }

    m_NameText.Draw(22, 0, checktrans);

    m_ButtonStatus->Draw(checktrans);

    const int valPosX = g_Config.ClientVersion > CV_200 ? 250 : 220;
    m_ValueText.Draw(valPosX - m_ValueText.Width, 0);

    RenderAdd_SetModelViewTranslation(
        g_renderCmdList, SetModelViewTranslationCmd{ { (float)-m_X, (float)-m_Y, 0.0f } });
}

bool CGUISkillItem::Select()
{
    int x = g_MouseManager.Position.X - m_X;
    int y = g_MouseManager.Position.Y - m_Y;

    return (x >= 0 && y >= 0 && x < 255 && y < 17);
}

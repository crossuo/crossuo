// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIShopResult.h"
#include "GUIMinMaxButtons.h"
#include "GUIShopItem.h"
#include "../OrionUO.h"
#include "../Point.h"
#include "../Managers/MouseManager.h"
#include "../Managers/FontsManager.h"

CGUIShopResult::CGUIShopResult(CGUIShopItem *shopItem, int x, int y)
    : CBaseGUI(GOT_SHOPRESULT, shopItem->Serial, shopItem->Graphic, shopItem->Color, x, y)
    , Price(shopItem->Price)
    , Name(shopItem->Name)
{
    DEBUG_TRACE_FUNCTION;
    MoveOnDrag = true;

    string name = Name + "\n" + "at " + std::to_string(Price) + " g.p.";
    g_FontManager.GenerateA(9, m_NameText, name, 0x021F, 100);

    int maxCount = shopItem->Count;

    if (maxCount > 999)
    {
        maxCount = 999;
    }

    m_MinMaxButtons =
        new CGUIMinMaxButtons(Serial, 0x0037, 156, m_NameText.Height / 2, 0, maxCount, 1);
    m_MinMaxButtons->DefaultTextOffset = -122;
    m_MinMaxButtons->SetTextParameters(true, STP_LEFT_CENTER, 9, 0x021F, false);
}

CGUIShopResult::~CGUIShopResult()
{
    DEBUG_TRACE_FUNCTION;
    m_NameText.Clear();
    RELEASE_POINTER(m_MinMaxButtons);
}

CBaseGUI *CGUIShopResult::SelectedItem()
{
    DEBUG_TRACE_FUNCTION;
    CBaseGUI *result = this;
    CSize size = m_MinMaxButtons->GetSize();

    if (g_Orion.PolygonePixelsInXY(
            m_X + m_MinMaxButtons->GetX(), m_Y + m_MinMaxButtons->GetY(), size.Width, size.Height))
    {
        result = m_MinMaxButtons;
    }

    return result;
}

void CGUIShopResult::PrepareTextures()
{
    DEBUG_TRACE_FUNCTION;
    m_MinMaxButtons->PrepareTextures();
}

void CGUIShopResult::Draw(bool checktrans)
{
    DEBUG_TRACE_FUNCTION;
    glTranslatef((GLfloat)m_X, (GLfloat)m_Y, 0.0f);

    glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);

    m_NameText.Draw(34, 0, checktrans);
    m_MinMaxButtons->Draw(checktrans);

    glTranslatef((GLfloat)-m_X, (GLfloat)-m_Y, 0.0f);
}

bool CGUIShopResult::Select()
{
    DEBUG_TRACE_FUNCTION;
    int x = g_MouseManager.Position.X - m_X;
    int y = g_MouseManager.Position.Y - m_Y;

    return (x >= 0 && y >= 0 && x < 200 && y < m_NameText.Height);
}

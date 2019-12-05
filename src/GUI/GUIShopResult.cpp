// MIT License
// Copyright (C) August 2016 Hotride

#include "GUIShopResult.h"
#include "GUIMinMaxButtons.h"
#include "GUIShopItem.h"
#include "../CrossUO.h"
#include "../Point.h"
#include "../Managers/MouseManager.h"
#include "../Managers/FontsManager.h"
#include "../Utility/PerfMarker.h"
#include "../Renderer/RenderAPI.h"

extern RenderCmdList *g_renderCmdList;

CGUIShopResult::CGUIShopResult(CGUIShopItem *shopItem, int x, int y)
    : CBaseGUI(GOT_SHOPRESULT, shopItem->Serial, shopItem->Graphic, shopItem->Color, x, y)
    , Price(shopItem->Price)
    , Name(shopItem->Name)
{
    MoveOnDrag = true;

    auto name = Name + "\n" + "at " + std::to_string(Price) + " g.p.";
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
    m_NameText.Clear();
    if (m_MinMaxButtons)
        delete m_MinMaxButtons;
    m_MinMaxButtons = nullptr;
}

CBaseGUI *CGUIShopResult::SelectedItem()
{
    CBaseGUI *result = this;
    CSize size = m_MinMaxButtons->GetSize();

    if (g_Game.PolygonePixelsInXY(
            m_X + m_MinMaxButtons->GetX(), m_Y + m_MinMaxButtons->GetY(), size.Width, size.Height))
    {
        result = m_MinMaxButtons;
    }

    return result;
}

void CGUIShopResult::PrepareTextures()
{
    m_MinMaxButtons->PrepareTextures();
}

void CGUIShopResult::Draw(bool checktrans)
{
    ScopedPerfMarker(__FUNCTION__);

#ifndef NEW_RENDERER_ENABLED
    glTranslatef((float)m_X, (float)m_Y, 0.0f);

    glUniform1iARB(g_ShaderDrawMode, SDM_NO_COLOR);
#else
    RenderAdd_SetModelViewTranslation(
        g_renderCmdList, SetModelViewTranslationCmd{ { (float)m_X, (float)m_Y, 0.0f } });

    ShaderUniformCmd cmd{ g_ShaderDrawMode, ShaderUniformType::ShaderUniformType_Int1 };
    cmd.value.asInt1 = SDM_NO_COLOR;
    RenderAdd_SetShaderUniform(g_renderCmdList, cmd);
#endif

    m_NameText.Draw(34, 0, checktrans);
    m_MinMaxButtons->Draw(checktrans);

#ifndef NEW_RENDERER_ENABLED
    glTranslatef((float)-m_X, (float)-m_Y, 0.0f);
#else
    RenderAdd_SetModelViewTranslation(
        g_renderCmdList, SetModelViewTranslationCmd{ { (float)-m_X, (float)-m_Y, 0.0f } });
#endif
}

bool CGUIShopResult::Select()
{
    int x = g_MouseManager.Position.X - m_X;
    int y = g_MouseManager.Position.Y - m_Y;

    return (x >= 0 && y >= 0 && x < 200 && y < m_NameText.Height);
}
